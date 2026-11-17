/* Escriptori — paquet «Xat». Xat per Wi-Fi: el lector serveix una pàgina web petita a la xarxa local i el mòbil o
   l'ordinador hi escriuen des del navegador. No hi ha xifratge ni contrasenya: qualsevol aparell de la mateixa xarxa
   que conegui l'adreça pot llegir i escriure. El servidor només funciona mentre l'Escriptori és obert. */
#define ESC_MODULE
#include "../mod.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <unistd.h>
enum { A_XAT_NEW = 100, A_XAT_CLEAR, A_XAT_WIFI, A_XAT_SRV };
#define XATFILE CFG "escriptori_xat.txt"
#define XPORT 8080
#define MAXMSG 200
#define TXT 300
typedef struct { char who; char name[24], tm[8], text[TXT]; } Msg;      /* who: 'L' = lector, 'W' = web */
static Msg *msgs; static int nmsg, loaded, base0, srv = -1, unread, toLast = 1, busy, wantSrv = 1;
static char kb[TXT];

static void clean(char *s) { for (; *s; s++) if (*s == '|' || (unsigned char)*s < 0x20) *s = ' '; }
static void x_save(void) {
  FILE *f = fopen(XATFILE, "w"); int i;
  if (!f) return;
  for (i = 0; i < nmsg; i++) fprintf(f, "%c|%s|%s|%s\n", msgs[i].who, msgs[i].name, msgs[i].tm, msgs[i].text);
  fclose(f);
}
static void x_load(void) {
  FILE *f; char ln[TXT + 80];
  if (loaded) return;
  loaded = 1; nmsg = 0;
  if (!msgs) msgs = (Msg *)calloc(MAXMSG, sizeof(Msg));
  if (!msgs || !(f = fopen(XATFILE, "r"))) return;
  while (nmsg < MAXMSG && fgets(ln, sizeof(ln), f)) {
    char *a, *b, *c; Msg *m = &msgs[nmsg];
    ln[strcspn(ln, "\r\n")] = 0;
    if (ln[0] != 'L' && ln[0] != 'W') continue;
    if (ln[1] != '|' || !(a = strchr(ln + 2, '|')) || !(b = strchr(a + 1, '|'))) continue;
    *a = 0; *b = 0; c = b + 1;
    m->who = ln[0]; snprintf(m->name, sizeof(m->name), "%s", ln + 2); snprintf(m->tm, sizeof(m->tm), "%s", a + 1); snprintf(m->text, sizeof(m->text), "%s", c);
    nmsg++;
  }
  fclose(f);
}
static void x_add(char who, const char *name, const char *text) {
  Msg *m; time_t now = time(NULL); struct tm *tm = localtime(&now);
  x_load();
  if (!msgs) return;
  if (nmsg >= MAXMSG) { memmove(&msgs[0], &msgs[1], sizeof(Msg) * (size_t)(MAXMSG - 1)); nmsg = MAXMSG - 1; base0++; }
  m = &msgs[nmsg++];
  m->who = who; snprintf(m->name, sizeof(m->name), "%s", name && name[0] ? name : "Mòbil"); snprintf(m->text, sizeof(m->text), "%s", text);
  snprintf(m->tm, sizeof(m->tm), "%d:%02d", tm->tm_hour, tm->tm_min);
  clean(m->name); clean(m->text);
  x_save();
}

/* ───────────── servidor HTTP mínim ───────────── */
static const char PAGE[] =
"<!doctype html><html lang='ca'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Xat amb el lector</title>"
"<style>body{margin:0;font:17px -apple-system,system-ui,sans-serif;background:#f4f4f2;color:#111;display:flex;flex-direction:column;height:100vh;height:100dvh}"
"header{padding:10px 16px;font-weight:700;border-bottom:1px solid #ccc;background:#fff;display:flex;gap:8px;align-items:center}"
"header label{font-weight:400;font-size:14px;margin-left:auto}header input{width:8em;font:inherit;border:1px solid #999;border-radius:8px;padding:6px 8px}"
"#l{flex:1;overflow:auto;padding:12px;display:flex;flex-direction:column;gap:8px}"
".m{max-width:80%;padding:8px 12px;border-radius:16px;background:#fff;border:1px solid #bbb;align-self:flex-start;white-space:pre-wrap;overflow-wrap:anywhere}"
".m.me{align-self:flex-end;background:#111;color:#fff;border-color:#111}.m small{display:block;font-size:12px;opacity:.75}"
"form{display:flex;gap:8px;padding:10px;background:#fff;border-top:1px solid #ccc}"
"form input{flex:1;min-width:0;font:inherit;padding:10px 12px;border:1px solid #999;border-radius:20px}"
"button{font:inherit;font-weight:700;padding:10px 16px;border:0;border-radius:20px;background:#111;color:#fff}"
"#e{padding:0 16px;font-size:13px;color:#a00000}</style></head><body>"
"<header><span>Xat amb el lector</span><label for='n'>Nom</label><input id='n' maxlength='20'></header>"
"<div id='l'></div><div id='e' role='status'></div>"
"<form id='f'><input id='t' maxlength='280' placeholder='Escriu un missatge' autocomplete='off' aria-label='Missatge'><button>Envia</button></form>"
"<script>var s=0,l=document.getElementById('l'),n=document.getElementById('n'),t=document.getElementById('t'),e=document.getElementById('e');"
"try{n.value=localStorage.getItem('xn')||''}catch(x){}if(!n.value)n.value='M\\u00f2bil';"
"function add(m){var d=document.createElement('div');d.className='m'+(m.w=='W'&&m.n==n.value?' me':'');var h=document.createElement('small');"
"h.textContent=m.n+' \\u00b7 '+m.h;d.appendChild(h);d.appendChild(document.createTextNode(m.t));l.appendChild(d)}"
"function poll(){fetch('/m?s='+s).then(function(r){return r.json()}).then(function(j){e.textContent='';if(j.next<s)l.textContent='';"
"j.m.forEach(add);if(j.m.length)l.scrollTop=l.scrollHeight;s=j.next}).catch(function(){"
"e.textContent='Sense connexi\\u00f3 amb el lector. Ha de tenir l\\u2019Escriptori obert, el Xat engegat i el Wi-Fi connectat.'})}"
"document.getElementById('f').onsubmit=function(v){v.preventDefault();var x=t.value.trim();if(!x)return;try{localStorage.setItem('xn',n.value)}catch(y){}"
"fetch('/s',{method:'POST',body:(n.value||'M\\u00f2bil')+'\\n'+x}).then(function(){t.value='';poll()}).catch(function(){e.textContent='No s\\u2019ha pogut enviar.'})};"
"poll();setInterval(poll,2500);</script></body></html>";

static void send_all(int fd, const char *b, size_t n) {
  while (n > 0) { ssize_t k = send(fd, b, n, MSG_NOSIGNAL); if (k <= 0) return; b += k; n -= (size_t)k; }
}
static void respond(int fd, const char *status, const char *ctype, const char *body, size_t len) {
  char h[256]; int n = snprintf(h, sizeof(h), "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", status, ctype, (unsigned long)len);
  send_all(fd, h, (size_t)n); if (len) send_all(fd, body, len);
}
static void send_msgs(int fd, long since) {
  size_t cap = (size_t)nmsg * (2 * TXT + 120) + 100, o = 0; char *j = (char *)malloc(cap); int i; long first;
  if (!j) { respond(fd, "500 Error", "text/plain", "", 0); return; }
  if (since < base0 || since > base0 + nmsg) since = base0;
  first = since - base0;
  o += (size_t)snprintf(j + o, cap - o, "{\"base\":%d,\"next\":%d,\"m\":[", base0, base0 + nmsg);
  for (i = (int)first; i < nmsg; i++) {
    const Msg *m = &msgs[i]; const char *f[3]; const char *k[3] = { "n", "h", "t" }; int q;
    f[0] = m->name; f[1] = m->tm; f[2] = m->text;
    o += (size_t)snprintf(j + o, cap - o, "%s{\"w\":\"%c\"", i > first ? "," : "", m->who);
    for (q = 0; q < 3; q++) {
      const char *p;
      o += (size_t)snprintf(j + o, cap - o, ",\"%s\":\"", k[q]);
      for (p = f[q]; *p && o + 4 < cap; p++) { if (*p == '"' || *p == '\\') j[o++] = '\\'; j[o++] = (unsigned char)*p < 0x20 ? ' ' : *p; }
      j[o++] = '"';
    }
    j[o++] = '}';
  }
  o += (size_t)snprintf(j + o, cap - o, "]}");
  respond(fd, "200 OK", "application/json; charset=utf-8", j, o);
  free(j);
}
/* Atén una connexió. Retorna 1 si ha entrat un missatge nou. */
static int handle(int fd) {
  char buf[4096]; int n = 0, cl = 0, got = 0; char *end = NULL, *p; struct timeval tv;
  tv.tv_sec = 0; tv.tv_usec = 400000; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  tv.tv_sec = 2; tv.tv_usec = 0; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  while (n < (int)sizeof(buf) - 1) {
    ssize_t k = recv(fd, buf + n, sizeof(buf) - 1 - (size_t)n, 0);
    if (k <= 0) break;
    n += (int)k; buf[n] = 0;
    if (!end && (end = strstr(buf, "\r\n\r\n"))) {
      end += 4;
      for (p = buf; p < end; p++) if (!strncasecmp(p, "\ncontent-length:", 16)) { cl = atoi(p + 16); break; }
      if (cl < 0 || cl > 2000) cl = 0;
    }
    if (end && n - (int)(end - buf) >= cl) break;
  }
  if (!end) return 0;
  x_load();
  if (!strncmp(buf, "GET / ", 6) || !strncmp(buf, "GET /index.html", 15)) respond(fd, "200 OK", "text/html; charset=utf-8", PAGE, sizeof(PAGE) - 1);
  else if (!strncmp(buf, "GET /m", 6)) { long s = 0; p = strstr(buf, "?s="); if (p && p < end) s = atol(p + 3); send_msgs(fd, s); }
  else if (!strncmp(buf, "POST /s", 7)) {
    char *nl; int bl = n - (int)(end - buf);
    if (bl > cl) bl = cl;
    end[bl] = 0; nl = strchr(end, '\n');
    if (nl) {
      char *t = nl + 1; size_t L;
      *nl = 0;
      while (*t == ' ' || *t == '\n' || *t == '\r') t++;
      L = strlen(t); while (L > 0 && (t[L - 1] == ' ' || t[L - 1] == '\n' || t[L - 1] == '\r')) t[--L] = 0;
      if (L > 0) { x_add('W', end, t); got = 1; }
    }
    respond(fd, got ? "200 OK" : "400 Bad Request", "text/plain; charset=utf-8", got ? "ok" : "buit", got ? 2 : 4);
  } else respond(fd, "404 Not Found", "text/plain; charset=utf-8", "no hi es", 8);
  return got;
}
static void xat_poll(void);
static void srv_stop(void) { if (srv >= 0 && H->api >= 6 && H->busy) H->busy("xat", NULL); ClearTimer(xat_poll); if (srv >= 0) close(srv); srv = -1; }
static void srv_start(void) {
  struct sockaddr_in a; int s, one = 1;
  if (srv >= 0 || H->api < 4) return;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(XPORT);
  if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(s, 8) < 0 || fcntl(s, F_SETFL, O_NONBLOCK) < 0) { close(s); return; }
  srv = s;
  SetWeakTimer("xat_poll", xat_poll, 700);
  if (H->api >= 6 && H->busy) H->busy("xat", "servidor web al port 8080");
}
static void on_unload(void) { srv_stop(); free(msgs); msgs = NULL; loaded = 0; }
/* Cada 0,7 s: atén les connexions pendents. Si arriba un missatge i el xat es veu, el redibuixa; si no, deixa un avís. */
static void xat_poll(void) {
  int k, got = 0;
  if (srv < 0) return;
  for (k = 0; k < 8; k++) { int c = accept(srv, NULL, NULL); if (c < 0) break; got += handle(c); close(c); }
  if (got && nmsg > 0) {
    if (!strcmp(H->front(), "xat")) { toLast = 1; if (!busy) H->redraw(0); }
    else {
      char t[120]; snprintf(t, sizeof(t), "%s: %s", msgs[nmsg - 1].name, msgs[nmsg - 1].text);
      unread += got; H->notify("Xat", "Missatge nou", t); H->status_changed();
    }
  }
  SetWeakTimer("xat_poll", xat_poll, 700);
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
static void v_xat(View *v) {
  int y = TOP, ch = 3 * U + U / 2, bh = 2 * U + U / 4, rh = 3 * U + U / 4, per, i, net = QueryNetwork(), wifi = (net & NET_WIFIREADY) != 0, pw = 5 * U, ph = 2 * U, bw; char sub[60], ip[64], url[100];
  x_load(); unread = 0;
  snprintf(sub, sizeof(sub), nmsg == 1 ? "%d missatge" : "%d missatges", nmsg);
  header("Xat", sub);
  ui_card(U / 2, y, SW - U, ch, UI_LINE);                                  /* targeta de connexió */
  if (H->api < 4) {
    SetFont(fText, BLACK); DrawTextRect(U + U / 4, y, SW - 2 * U - U / 2, ch, "Cal l'Escriptori 2.1 o posterior.\nActualitza el sistema a Configuració.", ALIGN_LEFT | VALIGN_MIDDLE);
  } else if (!wifi) {
    SetFont(fBold, BLACK); DrawTextRect(U + U / 4, y + U / 4, SW - 3 * U - pw, ch / 2 - U / 4, "El Wi-Fi no està connectat", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, DGRAY); DrawTextRect(U + U / 4, y + ch / 2, SW - 3 * U - pw, ch / 2 - U / 4, "Cal la mateixa xarxa que el mòbil o l'ordinador.", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    rfill(SW - U - pw, y + (ch - ph) / 2, pw, ph, ph / 2, BLACK); SetFont(fBold, WHITE); DrawTextRect(SW - U - pw, y + (ch - ph) / 2, pw, ph, "Connecta", ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(U / 2, y, SW - U, ch, A_XAT_WIFI, 0);
  } else {
    if (wantSrv) srv_start();
    if (srv >= 0 && my_ip(ip, sizeof(ip))) snprintf(url, sizeof(url), "http://%s:%d", ip, XPORT);
    else snprintf(url, sizeof(url), "%s", srv >= 0 ? "No s'ha trobat l'adreça del lector" : wantSrv ? "No s'ha pogut engegar el servidor" : "Servidor aturat");
    SetFont(fSmall, DGRAY); DrawTextRect(U + U / 4, y + U / 4, SW - 3 * U - pw, ch / 2 - U / 4, srv >= 0 ? "Obre aquesta adreça al navegador del mòbil o l'ordinador:" : "Engega'l per rebre missatges.", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fBig, BLACK); DrawTextRect(U + U / 4, y + ch / 2 - U / 8, SW - 3 * U - pw, ch / 2 - U / 8, url, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    rfill(SW - U - pw, y + (ch - ph) / 2, pw, ph, ph / 2, PAL(P_TILE)); SetFont(fBold, PAL(P_FG)); DrawTextRect(SW - U - pw, y + (ch - ph) / 2, pw, ph, srv >= 0 ? "Atura" : "Engega", ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(SW - U - pw - U / 2, y, pw + U, ch, A_XAT_SRV, 0);
  }
  y += ch + U / 3;
  bw = (SW - U - U / 3) * 2 / 3;
  button(U / 2, y, bw, bh, "Escriu un missatge", 1, A_XAT_NEW, 0);
  button(U / 2 + bw + U / 3, y, SW - U - bw - U / 3, bh, "Esborra-ho tot", 0, A_XAT_CLEAR, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = nmsg ? (nmsg + per - 1) / per : 1;
  if (toLast) { v->page = pages - 1; toLast = 0; }
  if (v->page >= pages) v->page = pages - 1;
  if (!nmsg) { SetFont(fText, DGRAY); DrawTextRect(U / 2, y, SW - U, 2 * U, "Encara no hi ha cap missatge.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < nmsg && i < (v->page + 1) * per; i++) {
    const Msg *m = &msgs[i]; int me = m->who == 'L', w = (SW - U) * 4 / 5, x = me ? SW - U / 2 - w : U / 2, h = rh - U / 4; char hd[48];
    ui_card(x, y, w, h, me ? UI_SOLID : UI_LINE);
    snprintf(hd, sizeof(hd), "%s · %s", me ? "Tu" : m->name, m->tm);
    SetFont(fSmall, me ? WHITE : DGRAY); DrawTextRect(x + U * 3 / 4, y + U / 6, w - U - U / 2, U, hd, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fText, me ? WHITE : BLACK); DrawTextRect(x + U * 3 / 4, y + U + U / 8, w - U - U / 2, h - U - U / 3, m->text, ALIGN_LEFT | VALIGN_TOP | DOTS);
    y += rh;
  }
}
static void on_kbd(char *text) {
  busy = 0;
  if (text) {
    char *s = text; size_t n;
    while (*s == ' ') s++;
    n = strlen(s); while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
    if (n > 0) { x_add('L', "Lector", s); toLast = 1; }
  }
  draw(1);
}
static void do_clear(void) { x_load(); base0 += nmsg; nmsg = 0; x_save(); toLast = 1; }
static void mod_icon(int x, int y, int s) { ic_path(IC_CHAT, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) { v_xat(v); }
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_XAT_NEW: busy = 1; kb[0] = 0; OpenKeyboard("Missatge", kb, TXT - 20, KBD_NORMAL, on_kbd); break;
    case A_XAT_CLEAR: confirm("Esborra la conversa", "S'esborraran tots els missatges desats al lector.", "Esborra", do_clear); break;
    case A_XAT_WIFI: NetConnect(NULL); draw(0); break;
    case A_XAT_SRV: if (srv >= 0) { srv_stop(); wantSrv = 0; } else { wantSrv = 1; srv_start(); } draw(0); break;
  }
}
static void mod_open(View *v) { (void)v; toLast = 1; unread = 0; if (H->api >= 4) H->on_unload("xat", on_unload); }
/* Ranura de la barra d'estat: bafarada amb el nombre de missatges sense llegir (només si n'hi ha). */
/* Ranura de la barra d'estat: icona I × I (I = 0,55·h) i, si cal, un número. */
static int si_w(int h) { char t[12]; int I = h * 55 / 100; if (!(unread > 0)) return 0; if (!(1)) return I; snprintf(t, sizeof(t), "%d", unread > 99 ? 99 : unread); SetFont(fSmall, BLACK); return I + U / 6 + StringWidth(t); }
static void si_d(int x, int y, int w, int h, int fg) {
  char t[12]; int I = h * 55 / 100;
  (void)w;
  ic_path(IC_CHAT, x, y + (h - I) / 2, I, 2.0, fg, SB_BG);
  if (1) { snprintf(t, sizeof(t), "%d", unread > 99 ? 99 : unread); SetFont(fSmall, fg); DrawTextRect(x + I + U / 6, y, StringWidth(t) + 2, h, t, ALIGN_LEFT | VALIGN_MIDDLE); }
}
static const StatusItem SI[1] = { { "noullegits", si_w, si_d } };
static Module M = { ESC_API_MIN, "xat", "Xat", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 1, SI, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
