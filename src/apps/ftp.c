/* Escriptori — paquet «FTP». Servidor FTP petit per passar fitxers entre el lector i l'ordinador o el mòbil per Wi-Fi.
   - Arrel: la memòria del lector. «system/» (dades i configuració) no es veu; «applications/» només es pot llegir.
   - Usuari «lector» i una contrasenya de 4 xifres que es crea la primera vegada i es desa a system/config/escriptori_ftp.txt.
   - Només mode passiu (PASV/EPSV), que és el que fan servir els clients actuals. Transferències sempre binàries.
   - FTP no xifra res: la contrasenya i els fitxers viatgen en clar per la xarxa local.
   - El lector fa una sola cosa alhora: mentre passa un fitxer, la pantalla no respon. */
#define ESC_MODULE
#include "../mod.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
enum { A_FTP_SRV = 100, A_FTP_WIFI, A_FTP_PIN };
#define FTPCFG CFG "escriptori_ftp.txt"
#define FPORT 2121
#define MAXS 4
#define LOGN 6
typedef struct {
  int fd, pasv, auth, user;
  char cwd[600], rnfr[700], in[1100];
  int inlen;
  time_t last;
} Sess;
static Sess ss[MAXS];
static int srv = -1, wantSrv = 1, busy, unseen;
static char pin[8], actlog[LOGN][120]; static int nlog;

static void ftp_log(const char *fmt, const char *a) {
  time_t now = time(NULL); struct tm *tm = localtime(&now); char t[100];
  snprintf(t, sizeof(t), fmt, a);
  if (nlog == LOGN) { memmove(actlog[0], actlog[1], sizeof(actlog[0]) * (LOGN - 1)); nlog--; }
  snprintf(actlog[nlog++], sizeof(actlog[0]), "%d:%02d  %s", tm->tm_hour, tm->tm_min, t);
}
static void pin_load(void) {
  FILE *f;
  if (pin[0]) return;
  if ((f = fopen(FTPCFG, "r"))) { if (fgets(pin, sizeof(pin), f)) pin[strcspn(pin, "\r\n")] = 0; fclose(f); }
  if (strlen(pin) != 4) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    snprintf(pin, sizeof(pin), "%04d", rand() % 10000);
    if ((f = fopen(FTPCFG, "w"))) { fprintf(f, "%s\n", pin); fclose(f); }
  }
}

/* ───────────── camins ─────────────
   Converteix un camí FTP (absolut o relatiu al directori actual) en un camí virtual net («/a/b») i el real.
   Retorna 0 si es pot llegir, 1 si a més es pot escriure, -1 si no és accessible. */
static int vres(const Sess *s, const char *arg, char *v, size_t vn, char *real, size_t rn) {
  char tmp[1300], *comp[64], *p, *tok; int n = 0, i, w = 1;
  if (!arg || !arg[0]) arg = ".";
  if (arg[0] == '/') snprintf(tmp, sizeof(tmp), "%s", arg); else snprintf(tmp, sizeof(tmp), "%s/%s", s->cwd, arg);
  for (tok = strtok_r(tmp, "/", &p); tok; tok = strtok_r(NULL, "/", &p)) {
    if (!strcmp(tok, ".") || !tok[0]) continue;
    if (!strcmp(tok, "..")) { if (n > 0) n--; continue; }
    if (tok[0] == '.' || n >= 63) return -1;                       /* fitxers ocults: fora */
    comp[n++] = tok;
  }
  if (n > 0 && !strcmp(comp[0], "system")) return -1;
  if (n > 0 && !strcmp(comp[0], "applications")) w = 0;
  if (n == 0) { snprintf(v, vn, "/"); }
  else { size_t o = 0; v[0] = 0; for (i = 0; i < n; i++) { int k = snprintf(v + o, vn - o, "/%s", comp[i]); if (k < 0 || (size_t)k >= vn - o) return -1; o += (size_t)k; } }
  if ((size_t)snprintf(real, rn, "%s%s", FLASHDIR, n ? v : "") >= rn) return -1;
  return w;
}

/* ───────────── xarxa ───────────── */
static void say1(int fd, const char *t) {
  char b[800]; int n = snprintf(b, sizeof(b), "%s\r\n", t); size_t o = 0;
  if (n > (int)sizeof(b)) n = (int)sizeof(b);
  while (o < (size_t)n) { ssize_t k = send(fd, b + o, (size_t)n - o, MSG_NOSIGNAL); if (k <= 0) { if (k < 0 && errno == EAGAIN) { usleep(2000); continue; } return; } o += (size_t)k; }
}
static int send_all(int fd, const char *b, size_t n) {
  while (n > 0) { ssize_t k = send(fd, b, n, MSG_NOSIGNAL); if (k <= 0) return -1; b += k; n -= (size_t)k; }
  return 0;
}
static void sess_close(Sess *s) { if (s->fd >= 0) close(s->fd); if (s->pasv >= 0) close(s->pasv); s->fd = s->pasv = -1; }
static int local_addr(int fd, struct sockaddr_in *a) { socklen_t l = sizeof(*a); return getsockname(fd, (struct sockaddr *)a, &l); }
/* Obre un port de dades passiu i en retorna el número (0 = error). */
static int pasv_open(Sess *s) {
  struct sockaddr_in a; int d;
  if (s->pasv >= 0) { close(s->pasv); s->pasv = -1; }
  if (local_addr(s->fd, &a) != 0) return 0;
  d = socket(AF_INET, SOCK_STREAM, 0);
  if (d < 0) return 0;
  a.sin_port = 0;
  if (bind(d, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(d, 1) < 0 || local_addr(d, &a) != 0) { close(d); return 0; }
  s->pasv = d;
  return ntohs(a.sin_port);
}
/* Espera la connexió de dades (fins a 8 s). */
static int data_accept(Sess *s) {
  struct pollfd p; int c; struct timeval tv;
  if (s->pasv < 0) return -1;
  p.fd = s->pasv; p.events = POLLIN; p.revents = 0;
  if (poll(&p, 1, 8000) <= 0) { close(s->pasv); s->pasv = -1; return -1; }
  c = accept(s->pasv, NULL, NULL);
  close(s->pasv); s->pasv = -1;
  if (c < 0) return -1;
  tv.tv_sec = 15; tv.tv_usec = 0;
  setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  return c;
}
static const char *MON[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static void do_list(Sess *s, const char *arg, int names) {
  char v[700], real[800], full[1200], line[1400]; struct stat st; DIR *d; struct dirent *e; int c, r;
  while (arg && *arg == '-') { while (*arg && *arg != ' ') arg++; while (*arg == ' ') arg++; }   /* «LIST -la» */
  r = vres(s, arg && *arg ? arg : ".", v, sizeof(v), real, sizeof(real));
  if (r < 0 || stat(real, &st) != 0) { say1(s->fd, "550 No existeix."); return; }
  say1(s->fd, "150 Llista.");
  if ((c = data_accept(s)) < 0) { say1(s->fd, "425 No s'ha pogut obrir la connexió de dades."); return; }
  d = S_ISDIR(st.st_mode) ? opendir(real) : NULL;
  while (d && (e = readdir(d))) {
    time_t now = time(NULL); struct tm tm;
    if (e->d_name[0] == '.' || (!strcmp(v, "/") && !strcmp(e->d_name, "system"))) continue;
    snprintf(full, sizeof(full), "%s/%s", real, e->d_name);
    if (stat(full, &st) != 0) continue;
    if (names) snprintf(line, sizeof(line), "%s\r\n", e->d_name);
    else {
      localtime_r(&st.st_mtime, &tm);
      if (now - st.st_mtime < 180L * 86400 && st.st_mtime <= now + 3600)
        snprintf(line, sizeof(line), "%s 1 lector lector %12lld %s %2d %02d:%02d %s\r\n", S_ISDIR(st.st_mode) ? "drwxr-xr-x" : "-rw-r--r--", (long long)st.st_size, MON[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, e->d_name);
      else
        snprintf(line, sizeof(line), "%s 1 lector lector %12lld %s %2d  %d %s\r\n", S_ISDIR(st.st_mode) ? "drwxr-xr-x" : "-rw-r--r--", (long long)st.st_size, MON[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900, e->d_name);
    }
    if (send_all(c, line, strlen(line)) < 0) break;
  }
  if (d) closedir(d);
  close(c);
  say1(s->fd, "226 Fet.");
}
static const char *base_name(const char *v) { const char *b = strrchr(v, '/'); return b ? b + 1 : v; }
static void do_retr(Sess *s, const char *arg) {
  char v[700], real[800], buf[16384]; struct stat st; int c, f; ssize_t k; int ok = 1;
  if (vres(s, arg, v, sizeof(v), real, sizeof(real)) < 0 || stat(real, &st) != 0 || !S_ISREG(st.st_mode) || (f = open(real, O_RDONLY)) < 0) { say1(s->fd, "550 No es pot llegir."); return; }
  say1(s->fd, "150 Enviant.");
  if ((c = data_accept(s)) < 0) { close(f); say1(s->fd, "425 No s'ha pogut obrir la connexió de dades."); return; }
  while ((k = read(f, buf, sizeof(buf))) > 0) if (send_all(c, buf, (size_t)k) < 0) { ok = 0; break; }
  close(f); close(c);
  if (ok) { say1(s->fd, "226 Fet."); ftp_log("Enviat: %s", base_name(v)); }
  else say1(s->fd, "426 S'ha tallat la transferència.");
}
/* Rep un fitxer en un temporal i el reanomena en acabar, perquè un fitxer a mitges no substitueixi mai el bo. */
static int do_stor(Sess *s, const char *arg) {
  char v[700], real[800], tmp[820], buf[16384]; int c, f, ok = 1; ssize_t k; long long tot = 0;
  if (vres(s, arg, v, sizeof(v), real, sizeof(real)) < 1 || !strcmp(v, "/")) { say1(s->fd, "553 Aquí no es pot escriure."); return 0; }
  snprintf(tmp, sizeof(tmp), "%s.part", real);
  if ((f = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) { say1(s->fd, "553 No es pot crear el fitxer."); return 0; }
  say1(s->fd, "150 Rebent.");
  if ((c = data_accept(s)) < 0) { close(f); unlink(tmp); say1(s->fd, "425 No s'ha pogut obrir la connexió de dades."); return 0; }
  for (;;) {
    k = recv(c, buf, sizeof(buf), 0);
    if (k == 0) break;
    if (k < 0) { if (errno == EINTR) continue; ok = 0; break; }
    if (write(f, buf, (size_t)k) != k) { ok = 0; break; }
    tot += k;
  }
  close(c);
  if (close(f) != 0) ok = 0;
  if (!ok || rename(tmp, real) != 0) { unlink(tmp); say1(s->fd, "451 No s'ha pogut desar (potser no hi ha espai)."); return 0; }
  (void)tot;
  say1(s->fd, "226 Desat.");
  ftp_log("Rebut: %s", base_name(v));
  return 1;
}
static int rm_tree_ok(const char *real) { return rmdir(real) == 0; }
/* Executa una ordre. Retorna 1 si ha arribat un fitxer nou. */
static int command(Sess *s, char *ln) {
  char *arg = strchr(ln, ' '), v[700], real[800], t[900]; struct stat st; int r, k;
  if (arg) { *arg++ = 0; while (*arg == ' ') arg++; } else arg = (char *)"";
  for (k = 0; ln[k]; k++) if (ln[k] >= 'a' && ln[k] <= 'z') ln[k] = (char)(ln[k] - 32);
  s->last = time(NULL);
  if (!strcmp(ln, "USER")) { s->user = !strcmp(arg, "lector"); s->auth = 0; say1(s->fd, "331 Contrasenya, si us plau."); return 0; }
  if (!strcmp(ln, "PASS")) {
    pin_load();
    if (s->user && !strcmp(arg, pin)) { s->auth = 1; say1(s->fd, "230 Benvinguda al lector."); ftp_log("%s", "S'ha connectat un aparell"); }
    else { say1(s->fd, "530 Usuari o contrasenya incorrectes."); usleep(300000); }
    return 0;
  }
  if (!strcmp(ln, "QUIT")) { say1(s->fd, "221 Adéu."); sess_close(s); return 0; }
  if (!strcmp(ln, "NOOP")) { say1(s->fd, "200 D'acord."); return 0; }
  if (!strcmp(ln, "SYST")) { say1(s->fd, "215 UNIX Type: L8"); return 0; }
  if (!strcmp(ln, "FEAT")) { say1(s->fd, "211-Funcions:\r\n UTF8\r\n SIZE\r\n MDTM\r\n EPSV\r\n PASV\r\n211 Fi."); return 0; }
  if (!strcmp(ln, "OPTS")) { say1(s->fd, "200 D'acord."); return 0; }
  if (!s->auth) { say1(s->fd, "530 Primer cal identificar-se."); return 0; }
  if (!strcmp(ln, "PWD") || !strcmp(ln, "XPWD")) { snprintf(t, sizeof(t), "257 \"%s\"", s->cwd); say1(s->fd, t); return 0; }
  if (!strcmp(ln, "CWD") || !strcmp(ln, "CDUP") || !strcmp(ln, "XCUP")) {
    if (strcmp(ln, "CWD")) arg = (char *)"..";
    if (vres(s, arg, v, sizeof(v), real, sizeof(real)) < 0 || stat(real, &st) != 0 || !S_ISDIR(st.st_mode)) { say1(s->fd, "550 No existeix."); return 0; }
    snprintf(s->cwd, sizeof(s->cwd), "%s", v); say1(s->fd, "250 D'acord."); return 0;
  }
  if (!strcmp(ln, "TYPE") || !strcmp(ln, "MODE") || !strcmp(ln, "STRU")) { say1(s->fd, "200 D'acord."); return 0; }
  if (!strcmp(ln, "PASV")) {
    struct sockaddr_in a; unsigned ip; int p = pasv_open(s);
    if (!p || local_addr(s->fd, &a) != 0) { say1(s->fd, "425 No es pot obrir el mode passiu."); return 0; }
    ip = ntohl(a.sin_addr.s_addr);
    snprintf(t, sizeof(t), "227 Mode passiu (%u,%u,%u,%u,%d,%d).", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, p >> 8, p & 255);
    say1(s->fd, t); return 0;
  }
  if (!strcmp(ln, "EPSV")) { int p = pasv_open(s); if (!p) { say1(s->fd, "425 No es pot obrir el mode passiu."); return 0; } snprintf(t, sizeof(t), "229 Mode passiu (|||%d|)", p); say1(s->fd, t); return 0; }
  if (!strcmp(ln, "PORT") || !strcmp(ln, "EPRT")) { say1(s->fd, "502 Només mode passiu."); return 0; }
  if (!strcmp(ln, "LIST")) { do_list(s, arg, 0); return 0; }
  if (!strcmp(ln, "NLST")) { do_list(s, arg, 1); return 0; }
  if (!strcmp(ln, "RETR")) { do_retr(s, arg); return 0; }
  if (!strcmp(ln, "STOR")) return do_stor(s, arg);
  if (!strcmp(ln, "SIZE") || !strcmp(ln, "MDTM")) {
    if (vres(s, arg, v, sizeof(v), real, sizeof(real)) < 0 || stat(real, &st) != 0 || !S_ISREG(st.st_mode)) { say1(s->fd, "550 No existeix."); return 0; }
    if (ln[0] == 'S') snprintf(t, sizeof(t), "213 %lld", (long long)st.st_size);
    else { struct tm tm; gmtime_r(&st.st_mtime, &tm); snprintf(t, sizeof(t), "213 %04d%02d%02d%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); }
    say1(s->fd, t); return 0;
  }
  r = vres(s, arg, v, sizeof(v), real, sizeof(real));
  if (!strcmp(ln, "DELE")) { if (r < 1 || unlink(real) != 0) say1(s->fd, "550 No s'ha pogut esborrar."); else { say1(s->fd, "250 Esborrat."); ftp_log("Esborrat: %s", base_name(v)); } return 0; }
  if (!strcmp(ln, "MKD") || !strcmp(ln, "XMKD")) { if (r < 1 || mkdir(real, 0755) != 0) say1(s->fd, "550 No s'ha pogut crear."); else { snprintf(t, sizeof(t), "257 \"%s\" creada.", v); say1(s->fd, t); } return 0; }
  if (!strcmp(ln, "RMD") || !strcmp(ln, "XRMD")) { if (r < 1 || !strcmp(v, "/") || !rm_tree_ok(real)) say1(s->fd, "550 No s'ha pogut esborrar (ha d'estar buida)."); else say1(s->fd, "250 Esborrada."); return 0; }
  if (!strcmp(ln, "RNFR")) { if (r < 1 || stat(real, &st) != 0) { say1(s->fd, "550 No existeix."); s->rnfr[0] = 0; } else { snprintf(s->rnfr, sizeof(s->rnfr), "%s", real); say1(s->fd, "350 I el nom nou?"); } return 0; }
  if (!strcmp(ln, "RNTO")) { if (!s->rnfr[0] || r < 1 || rename(s->rnfr, real) != 0) say1(s->fd, "550 No s'ha pogut reanomenar."); else say1(s->fd, "250 Reanomenat."); s->rnfr[0] = 0; return 0; }
  say1(s->fd, "502 Ordre no disponible.");
  return 0;
}

/* ───────────── bucle ─────────────
   Cada 0,5 s es miren les connexions. Si hi ha activitat, es continua atenent fins que el client calla una estona
   (màxim 3 s seguits, o el que duri una transferència), perquè connectar-se no trigui segons. */
static void ftp_poll(void);
static void srv_stop(void) { int i; ClearTimer(ftp_poll); for (i = 0; i < MAXS; i++) sess_close(&ss[i]); if (srv >= 0) close(srv); srv = -1; }
static void srv_start(void) {
  struct sockaddr_in a; int s, one = 1, i;
  if (srv >= 0 || H->api < 4) return;
  pin_load();
  for (i = 0; i < MAXS; i++) { ss[i].fd = ss[i].pasv = -1; }
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(FPORT);
  if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(s, 4) < 0 || fcntl(s, F_SETFL, O_NONBLOCK) < 0) { close(s); return; }
  srv = s;
  SetWeakTimer("ftp_poll", ftp_poll, 500);
}
static void on_unload(void) { srv_stop(); }
static int nsess(void) { int i, n = 0; for (i = 0; i < MAXS; i++) if (ss[i].fd >= 0 && ss[i].auth) n++; return n; }
static void ftp_poll(void) {
  struct timeval t0, t1; int got = 0, before = nsess(), nlogBefore = nlog, i;
  if (srv < 0) return;
  gettimeofday(&t0, NULL);
  for (;;) {
    struct pollfd p[MAXS + 1]; int n = 0, map[MAXS + 1], rc;
    p[n].fd = srv; p[n].events = POLLIN; p[n].revents = 0; map[n++] = -1;
    for (i = 0; i < MAXS; i++) if (ss[i].fd >= 0) { p[n].fd = ss[i].fd; p[n].events = POLLIN; p[n].revents = 0; map[n++] = i; }
    rc = poll(p, (nfds_t)n, 150);
    if (rc <= 0) break;
    if (p[0].revents & POLLIN) {
      int c = accept(srv, NULL, NULL);
      if (c >= 0) {
        for (i = 0; i < MAXS && ss[i].fd >= 0; i++) {}
        if (i == MAXS) { say1(c, "421 Massa connexions."); close(c); }
        else {
          memset(&ss[i], 0, sizeof(Sess)); ss[i].fd = c; ss[i].pasv = -1; snprintf(ss[i].cwd, sizeof(ss[i].cwd), "/"); ss[i].last = time(NULL);
          fcntl(c, F_SETFL, O_NONBLOCK);
          say1(c, "220 Lector PocketBook (Escriptori). Usuari: lector");
        }
      }
    }
    for (i = 1; i < n; i++) {
      Sess *s = &ss[map[i]]; ssize_t k; char *nl;
      if (!(p[i].revents & (POLLIN | POLLHUP | POLLERR)) || s->fd < 0) continue;
      k = recv(s->fd, s->in + s->inlen, sizeof(s->in) - 1 - (size_t)s->inlen, 0);
      if (k <= 0) { if (k < 0 && errno == EAGAIN) continue; sess_close(s); continue; }
      s->inlen += (int)k; s->in[s->inlen] = 0;
      while (s->fd >= 0 && (nl = strchr(s->in, '\n'))) {
        char line[1100]; size_t L = (size_t)(nl - s->in);
        memcpy(line, s->in, L); line[L] = 0; if (L && line[L - 1] == '\r') line[L - 1] = 0;
        memmove(s->in, nl + 1, (size_t)s->inlen - L); s->inlen -= (int)L + 1;
        if (line[0]) got += command(s, line);
      }
      if (s->fd >= 0 && s->inlen >= (int)sizeof(s->in) - 1) { say1(s->fd, "500 Línia massa llarga."); sess_close(s); }
    }
    gettimeofday(&t1, NULL);
    if ((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000 > 3000) break;
  }
  for (i = 0; i < MAXS; i++) if (ss[i].fd >= 0 && time(NULL) - ss[i].last > 600) sess_close(&ss[i]);   /* 10 min sense res */
  if (got && strcmp(H->front(), "ftp")) {
    char t[140]; const char *m = nlog ? strstr(actlog[nlog - 1], "  ") : NULL; snprintf(t, sizeof(t), got == 1 ? "%s" : "%s i més", m ? m + 2 : "");
    unseen += got; H->notify("FTP", got == 1 ? "Fitxer rebut" : "Fitxers rebuts", t); H->status_changed();
  }
  if (!strcmp(H->front(), "ftp") && !busy && (nlog != nlogBefore || nsess() != before)) H->redraw(0);
  SetWeakTimer("ftp_poll", ftp_poll, 500);
}
static int my_ip(char *out, int sz) {
  struct ifaddrs *ifa, *p; int ok = 0;
  out[0] = 0;
  if (getifaddrs(&ifa) != 0) return 0;
  for (p = ifa; p && !ok; p = p->ifa_next) {
    struct sockaddr_in *sa = (struct sockaddr_in *)p->ifa_addr;
    if (!sa || sa->sin_family != AF_INET || (ntohl(sa->sin_addr.s_addr) >> 24) == 127) continue;
    if (inet_ntop(AF_INET, &sa->sin_addr, out, (socklen_t)sz)) ok = 1;
  }
  freeifaddrs(ifa);
  return ok;
}

/* ───────────── pantalla ───────────── */
static void kvrow(int y, int rh, int first, const char *k, const char *v, int big) {
  if (!first) FillArea(U, y, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fText, PAL(P_SUB)); DrawTextRect(U + U / 4, y, SW / 3, rh, k, ALIGN_LEFT | VALIGN_MIDDLE);
  SetFont(big ? fBig : fBold, BLACK); DrawTextRect(SW / 3, y, SW * 2 / 3 - U - U / 4, rh, v, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
}
static void v_ftp(View *v) {
  int y = TOP, rh = 2 * U + U / 4, net = QueryNetwork(), wifi = (net & NET_WIFIREADY) != 0, bh = 2 * U + U / 2, i, n; char ip[64], t[120];
  (void)v; unseen = 0; pin_load();
  if (wifi && wantSrv) srv_start();
  n = nsess();
  if (srv < 0) snprintf(t, sizeof(t), "Aturat");
  else if (n) snprintf(t, sizeof(t), n == 1 ? "%d aparell connectat" : "%d aparells connectats", n);
  else snprintf(t, sizeof(t), "Esperant connexions");
  header("FTP", t);
  if (H->api < 4) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 4 * U, "Cal l'Escriptori 2.1 o posterior.\nActualitza el sistema a Configuració.", ALIGN_LEFT | VALIGN_TOP); return; }
  if (!wifi) {
    ui_card(U / 2, y, SW - U, 4 * U, UI_LINE);
    SetFont(fBold, BLACK); DrawTextRect(U + U / 4, y + U / 3, SW - 3 * U, 3 * U / 2, "El Wi-Fi no està connectat", ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fSmall, DGRAY); DrawTextRect(U + U / 4, y + 2 * U, SW - 3 * U, 3 * U / 2, "L'aparell que hi vulgui entrar ha de ser a la mateixa xarxa.", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    y += 4 * U + U / 3;
    button(U / 2, y, SW - U, bh, "Connecta el Wi-Fi", 1, A_FTP_WIFI, 0);
    return;
  }
  SetFont(fSmall, DGRAY); DrawTextRect(U, y, SW - 2 * U, U + U / 4, "CONNEXIÓ", ALIGN_LEFT | VALIGN_MIDDLE);
  y += U + U / 4;
  ui_card(U / 2, y, SW - U, 4 * rh, UI_LINE);
  if (srv >= 0 && my_ip(ip, sizeof(ip))) snprintf(t, sizeof(t), "%s", ip); else snprintf(t, sizeof(t), "%s", srv >= 0 ? "no trobada" : "—");
  kvrow(y, rh, 1, "Servidor", t, 1);
  snprintf(t, sizeof(t), "%d", FPORT); kvrow(y + rh, rh, 0, "Port", t, 0);
  kvrow(y + 2 * rh, rh, 0, "Usuari", "lector", 0);
  kvrow(y + 3 * rh, rh, 0, "Contrasenya", pin, 1);
  y += 4 * rh + U / 3;
  SetFont(fSmall, DGRAY);
  DrawTextRect(U / 2, y, SW - U, 3 * U, "Mac: Finder › Ves › Connecta't al servidor (només per llegir) o Cyberduck. iPhone: una app amb FTP. Fes servir el mode passiu.", ALIGN_LEFT | VALIGN_TOP);
  y += 3 * U;
  { int bw = (SW - U - U / 3) / 2;
    button(U / 2, y, bw, bh, srv >= 0 ? "Atura el servidor" : "Engega el servidor", srv < 0, A_FTP_SRV, 0);
    button(U / 2 + bw + U / 3, y, bw, bh, "Canvia la contrasenya", 0, A_FTP_PIN, 0); }
  y += bh + U / 2;
  SetFont(fSmall, DGRAY); DrawTextRect(U, y, SW - 2 * U, U + U / 4, "ACTIVITAT", ALIGN_LEFT | VALIGN_MIDDLE);
  y += U + U / 4;
  if (!nlog) { SetFont(fText, DGRAY); DrawTextRect(U, y, SW - 2 * U, 2 * U, "Encara no hi ha hagut res.", ALIGN_LEFT | VALIGN_MIDDLE); }
  for (i = nlog - 1; i >= 0 && y + rh <= BOTTOM; i--) {
    if (i < nlog - 1) FillArea(U, y, SW - 2 * U, 2, LGRAY);
    SetFont(fText, BLACK); DrawTextRect(U, y, SW - 2 * U, rh, actlog[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    y += rh;
  }
}
static void pin_new(void) {
  FILE *f; int i;
  srand((unsigned)time(NULL) ^ (unsigned)rand());
  snprintf(pin, sizeof(pin), "%04d", rand() % 10000);
  if ((f = fopen(FTPCFG, "w"))) { fprintf(f, "%s\n", pin); fclose(f); }
  for (i = 0; i < MAXS; i++) sess_close(&ss[i]);                 /* qui estava connectat ha de tornar a entrar */
  ftp_log("%s", "Contrasenya nova");
}
static void mod_icon(int x, int y, int s) { ic_path(IC_FTP, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) { v_ftp(v); }
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_FTP_SRV: if (srv >= 0) { srv_stop(); wantSrv = 0; ftp_log("%s", "Servidor aturat"); } else { wantSrv = 1; srv_start(); } draw(0); break;
    case A_FTP_WIFI: NetConnect(NULL); draw(0); break;
    case A_FTP_PIN: confirm("Contrasenya nova", "Es crearà una contrasenya nova i es tallaran les connexions obertes.", "Canvia-la", pin_new); break;
  }
}
static void mod_open(View *v) {
  (void)v; unseen = 0;
  if (H->api < 4) return;
  H->on_unload("ftp", on_unload);
  if (wantSrv && (QueryNetwork() & NET_WIFIREADY)) srv_start();      /* abans de dibuixar, perquè la barra ja el mostri */
}
/* Ranura de la barra d'estat: mentre el servidor és engegat, una fletxa doble; amb fitxers nous, també el nombre. */
/* Ranura de la barra d'estat: icona I × I (I = 0,55·h) i, si cal, un número. */
static int si_w(int h) { char t[12]; int I = h * 55 / 100; if (!(srv >= 0)) return 0; if (!(unseen > 0)) return I; snprintf(t, sizeof(t), "%d", unseen > 99 ? 99 : unseen); SetFont(fSmall, BLACK); return I + U / 6 + StringWidth(t); }
static void si_d(int x, int y, int w, int h, int fg) {
  char t[12]; int I = h * 55 / 100;
  (void)w;
  ic_path(IC_UPDOWN, x, y + (h - I) / 2, I, 2.0, fg, SB_BG);
  if (unseen > 0) { snprintf(t, sizeof(t), "%d", unseen > 99 ? 99 : unseen); SetFont(fSmall, fg); DrawTextRect(x + I + U / 6, y, StringWidth(t) + 2, h, t, ALIGN_LEFT | VALIGN_MIDDLE); }
}
static const StatusItem SI[1] = { { "servidor", si_w, si_d } };
static Module M = { ESC_API_MIN, "ftp", "FTP", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 1, SI, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
