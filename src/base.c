/* Escriptori — sistema base: escriptori, barra d'estat, gestor de paquets i actualització. */
#include "mod.h"
#include <dlfcn.h>
#include <sys/statvfs.h>
#include <limits.h>

#define APP_BUILD 5
#define APP_VERNAME "1.1"
#define UPD_BASE "https://raw.githubusercontent.com/sarnaloka/pb_launcher/main/"
#define SYSDIR FLASHDIR "/system/escriptori"
#define PKGDIR SYSDIR "/packages"

enum { V_DESK, V_STORE, V_PKG, V_SYSTEM, V_QUICK, V_HOME };
enum { A_NONE, A_BACK, A_HOME, A_PREV, A_NEXT, A_EXIT, A_APP, A_STORE_REFRESH, A_STORE_ALL, A_PKG, A_PKG_INSTALL, A_PKG_REMOVE,
       A_UPD_CHECK, A_UPD_INSTALL, A_WIFI,
       A_QUICK, A_WIDGET, A_QWIFI, A_QLIGHT_DN, A_QLIGHT_UP, A_QCLEAN, A_QGO, A_HE_TOGGLE, A_HE_UP, A_HE_DN };          /* les accions dels paquets comencen a 100 */
typedef struct { int x, y, w, h, act, arg; } Hit;
#define MAXSTACK 24
#define MAXHITS 400
static View stack[MAXSTACK];
static int depth;
static Hit hits[MAXHITS];
static int nhits;
static int SW, SH, U, SB;
static ifont *fTitle, *fBold, *fText, *fSmall, *fBig, *fHuge;
static int pages = 1;
/* L'actualitzador de la compilació 1 rebutja binaris de menys de 100 kB. Aquest farciment fa que la
   transició 1 -> 0.8 sigui possible; es pot treure quan ja no quedi cap lector amb la compilació 1. */
__attribute__((used)) static const char transition_padding[90000] = { 1 };
static const char *pendMsg;
static void say(const char *m) { pendMsg = m; }

#include "base_gen.h"
#include "ui.h"

/* ───────────── paquets carregats ───────────── */
typedef struct { void *dl; Module *m; char id[48]; } Loaded;
#define MAXMODS 64
static Loaded mods[MAXMODS]; static int nmods;
static Host host;
static int mod_find(const char *id) { int i; for (i = 0; i < nmods; i++) if (!strcmp(mods[i].id, id)) return i; return -1; }
static void drop_module_views(void) { int k; for (k = 0; k < depth; k++) if (stack[k].mod >= 0) { depth = k > 0 ? k : 1; break; } }
static int mod_load(const char *id) {
  char path[700]; void *dl; Module *(*fn)(const Host *); Module *m; int i;
  if (nmods >= MAXMODS || mod_find(id) >= 0) return -1;
  snprintf(path, sizeof(path), PKGDIR "/%s.pkg", id);
  dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!dl) return -1;
  fn = (Module *(*)(const Host *))dlsym(dl, "escriptori_module");
  m = fn ? fn(&host) : NULL;
  if (!m || m->api != ESC_API || !m->fn_draw || !m->name) { dlclose(dl); return -2; }
  drop_module_views();
  for (i = nmods; i > 0 && strcasecmp(mods[i - 1].m->name, m->name) > 0; i--) mods[i] = mods[i - 1];
  mods[i].dl = dl; mods[i].m = m; snprintf(mods[i].id, sizeof(mods[i].id), "%s", id);
  nmods++;
  return 0;
}
static void mod_unload(int i) {
  if (i < 0 || i >= nmods) return;
  drop_module_views();
  dlclose(mods[i].dl);
  memmove(&mods[i], &mods[i + 1], sizeof(Loaded) * (size_t)(nmods - i - 1));
  nmods--;
}
static int status_modules(const StatusItem **out, int max) {
  int i, j, n = 0;
  for (i = 0; i < nmods; i++) for (j = 0; j < mods[i].m->nstatus && n < max; j++) out[n++] = &mods[i].m->status[j];
  return n;
}
static int pkgs_on_disk(void) {
  DIR *d = opendir(PKGDIR); struct dirent *e; int n = 0;
  if (!d) return 0;
  while ((e = readdir(d))) { size_t l = strlen(e->d_name); if (l > 4 && e->d_name[0] != '.' && !strcmp(e->d_name + l - 4, ".pkg")) n++; }
  closedir(d);
  return n;
}
static void mods_scan(void) {
  DIR *d = opendir(PKGDIR); struct dirent *e; char id[48];
  if (!d) return;
  while ((e = readdir(d))) {
    size_t n = strlen(e->d_name);
    if (n < 5 || n > 44 || e->d_name[0] == '.' || strcmp(e->d_name + n - 4, ".pkg")) continue;
    memcpy(id, e->d_name, n - 4); id[n - 4] = 0;
    mod_load(id);
  }
  closedir(d);
}

/* ───────────── catàleg i instal·lació ───────────── */
typedef struct { char id[48], name[64], desc[200]; int ver; long size; unsigned long crc; } Pkg;
#define MAXCAT 64
static Pkg cat[MAXCAT]; static int ncat;
static void cat_parse(char *text) {       /* línies: id|nom|versió|mida|crc|descripció */
  char *ln = text;
  ncat = 0;
  while (ln && *ln && ncat < MAXCAT) {
    char *nx = strchr(ln, '\n'), *f[6]; int k = 0; char *p = ln;
    if (nx) *nx = 0;
    while (k < 6) { f[k++] = p; p = strchr(p, '|'); if (!p) break; *p++ = 0; }
    if (k == 6 && f[0][0] && f[0][0] != '#' && strlen(f[0]) < 44 && !strpbrk(f[0], "/\\. ")) {
      Pkg *q = &cat[ncat]; char *e = strpbrk(f[5], "\r\n");
      if (e) *e = 0;
      snprintf(q->id, sizeof(q->id), "%s", f[0]); snprintf(q->name, sizeof(q->name), "%s", f[1]); snprintf(q->desc, sizeof(q->desc), "%s", f[5]);
      q->ver = atoi(f[2]); q->size = atol(f[3]); q->crc = strtoul(f[4], NULL, 16);
      if (q->ver > 0 && q->size > 0) ncat++;
    }
    ln = nx ? nx + 1 : NULL;
  }
}
static char *slurp(const char *path, long *len) {
  FILE *f = fopen(path, "rb"); long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 1024 * 1024) { fclose(f); return NULL; }
  b = (char *)malloc((size_t)n + 1);
  if (b) { n = (long)fread(b, 1, (size_t)n, f); b[n] = 0; if (len) *len = n; }
  fclose(f);
  return b;
}
static void cat_load(void) { char *b = slurp(PKGDIR "/index.txt", NULL); ncat = 0; if (b) { cat_parse(b); free(b); } }
static int pkg_present(const char *id) { char p[700]; struct stat st; snprintf(p, sizeof(p), PKGDIR "/%s.pkg", id); return stat(p, &st) == 0; }
static int pkg_ver(const char *id) {      /* 0 = no instal·lat */
  char p[700], *b; int v;
  if (!pkg_present(id)) return 0;
  snprintf(p, sizeof(p), PKGDIR "/%s.ver", id);
  b = slurp(p, NULL); v = b ? atoi(b) : 1; free(b);
  return v > 0 ? v : 1;
}
static int write_file(const char *path, const char *data, long len) {
  char part[720]; FILE *f; size_t w;
  snprintf(part, sizeof(part), "%s.part", path);
  f = fopen(part, "wb");
  if (!f) return -1;
  w = fwrite(data, 1, (size_t)len, f);
  if (fclose(f) != 0 || w != (size_t)len) { remove(part); return -1; }
  if (rename(part, path) != 0) { remove(path); if (rename(part, path) != 0) { remove(part); return -1; } }
  return 0;
}
static int store_refresh(void) {
  char url[300], *b; int len = 0;
  snprintf(url, sizeof(url), UPD_BASE "packages/index.txt?t=%ld&r=%d", (long)time(NULL), rand());
  b = net_get_t(url, &len, 25);
  if (!b) return -1;
  if (!strchr(b, '|')) { free(b); return -2; }
  mkdir(SYSDIR, 0777); mkdir(PKGDIR, 0777);
  write_file(PKGDIR "/index.txt", b, len);
  free(b);
  cat_load();
  return 0;
}
/* 0 correcte; -1 xarxa; -2 la descàrrega no quadra; -3 no es pot desar; -4 no es pot carregar */
static int pkg_install(const Pkg *q) {
  char url[300], path[700], ver[32], *b; int len = 0, i;
  snprintf(url, sizeof(url), UPD_BASE "packages/%s.pkg?t=%ld&r=%d", q->id, (long)time(NULL), rand());
  b = net_get_t(url, &len, 180);
  if (!b) return -1;
  if ((long)len != q->size || crc32_buf((const unsigned char *)b, len) != q->crc || len < 1000 || memcmp(b, "\177ELF", 4)) { free(b); return -2; }
  mkdir(SYSDIR, 0777); mkdir(PKGDIR, 0777);
  i = mod_find(q->id);
  if (i >= 0) mod_unload(i);
  snprintf(path, sizeof(path), PKGDIR "/%s.pkg", q->id);
  if (write_file(path, b, len) != 0) { free(b); mod_load(q->id); return -3; }
  free(b);
  chmod(path, 0755);
  snprintf(ver, sizeof(ver), "%d\n", q->ver);
  snprintf(path, sizeof(path), PKGDIR "/%s.ver", q->id);
  write_file(path, ver, (long)strlen(ver));
  return mod_load(q->id) == 0 ? 0 : -4;
}
static const char *pkg_err(int rc) {
  return rc == -1 ? "No s'ha pogut descarregar. Comprova el Wi-Fi." : rc == -2 ? "La descàrrega no coincideix amb el catàleg. No s'ha instal·lat." :
         rc == -3 ? "No s'ha pogut desar el paquet al lector." : "El paquet s'ha baixat però no és compatible amb aquesta versió del sistema.";
}
static int pkgSel = -1;
static void pkg_remove(void) {
  char path[700]; int i;
  if (pkgSel < 0 || pkgSel >= ncat) return;
  i = mod_find(cat[pkgSel].id);
  if (i >= 0) mod_unload(i);
  snprintf(path, sizeof(path), PKGDIR "/%s.pkg", cat[pkgSel].id); remove(path);
  snprintf(path, sizeof(path), PKGDIR "/%s.ver", cat[pkgSel].id); remove(path);
}
static const char *pkg_state(const Pkg *q) {
  int v = pkg_ver(q->id);
  if (!v) return "No instal·lat";
  if (mod_find(q->id) < 0) return "Incompatible: cal actualitzar-lo";
  return v < q->ver ? "Hi ha una versió nova" : "Instal·lat";
}
static void fmt_size(char *out, int sz, long b) {
  if (b < 1024 * 1024) snprintf(out, (size_t)sz, "%ld kB", (b + 1023) / 1024);
  else snprintf(out, (size_t)sz, "%ld,%ld MB", b / (1024 * 1024), (b % (1024 * 1024)) * 10 / (1024 * 1024));
}

/* ───────────── actualització del sistema ───────────── */
static int updState;            /* 0 sense comprovar, 1 al dia, 2 hi ha versió nova, 3 instal·lada, -1 error */
static int updBuild; static long updSize; static unsigned long updCrc; static char updNotes[300], updName[32], selfPath[600];
/* version.txt: 1 compilació, 2 mida, 3 CRC32 hex, 4 novetats, 5 nom de la versió */
static void upd_check(void) {
  char url[300], *b, *l[5]; int k;
  snprintf(url, sizeof(url), UPD_BASE "version.txt?t=%ld&r=%d", (long)time(NULL), rand());
  b = net_get_t(url, NULL, 25);
  updState = -1; updNotes[0] = 0; updName[0] = 0;
  if (!b) { say("No s'ha pogut consultar la versió. Comprova el Wi-Fi."); return; }
  l[0] = b;
  for (k = 1; k < 5; k++) { l[k] = l[k - 1] ? strchr(l[k - 1], '\n') : NULL; if (l[k]) *l[k]++ = 0; }
  updBuild = atoi(l[0]);
  if (updBuild <= 0 || !l[1] || !l[2]) { free(b); say("La informació de versió del servidor no és vàlida."); return; }
  updSize = atol(l[1]); updCrc = strtoul(l[2], NULL, 16);
  if (l[3]) { snprintf(updNotes, sizeof(updNotes), "%s", l[3]); updNotes[strcspn(updNotes, "\r\n")] = 0; }
  if (l[4]) { snprintf(updName, sizeof(updName), "%s", l[4]); updName[strcspn(updName, "\r\n")] = 0; }
  if (!updName[0]) snprintf(updName, sizeof(updName), "%d", updBuild);
  free(b);
  updState = updBuild > APP_BUILD ? 2 : 1;
}
static void upd_install(void) {
  char url[300], *b; int len = 0;
  snprintf(url, sizeof(url), UPD_BASE "Escriptori.app?t=%ld&r=%d", (long)time(NULL), rand());
  b = net_get_t(url, &len, 180);
  if (!b) { say("No s'ha pogut descarregar l'actualització."); return; }
  if ((long)len != updSize || crc32_buf((const unsigned char *)b, len) != updCrc || len < 20000 || memcmp(b, "\177ELF", 4)) {
    free(b); say("La descàrrega no coincideix amb la versió publicada. No s'ha canviat res."); return;
  }
  if (write_file(selfPath, b, len) != 0) { free(b); say("No s'ha pogut substituir l'aplicació. No s'ha canviat res."); return; }
  free(b);
  chmod(selfPath, 0755);
  updState = 3;
}

/* ───────────── vistes del sistema base ───────────── */
static void icon_store(int x, int y, int s) {
  int t = s / 12 > 4 ? s / 12 : 5;
  FillArea(x + s / 6, y + s / 3, s * 2 / 3, s / 2, BLACK); FillArea(x + s / 6 + t, y + s / 3 + t, s * 2 / 3 - 2 * t, s / 2 - 2 * t, WHITE);
  thick(x + s / 2, y + s / 8, x + s / 2, y + s * 5 / 8, t, BLACK);
  thick(x + s / 2, y + s * 5 / 8, x + s * 3 / 8, y + s / 2, t, BLACK); thick(x + s / 2, y + s * 5 / 8, x + s * 5 / 8, y + s / 2, t, BLACK);
}
static void icon_system(int x, int y, int s) {
  int k, t = s / 8 > 5 ? s / 8 : 6;
  for (k = 0; k < 4; k++) {
    static const int DX[4] = { 100, 71, 0, -71 }, DY[4] = { 0, 71, 100, 71 };
    int dx = DX[k] * s * 3 / 800, dy = DY[k] * s * 3 / 800;
    thick(x + s / 2 - dx, y + s / 2 - dy, x + s / 2 + dx, y + s / 2 + dy, t, BLACK);
  }
  disc(x + s / 2, y + s / 2, s * 9 / 32, BLACK); disc(x + s / 2, y + s / 2, s / 8, WHITE);
}
static const char *WDAYB[7] = { "diumenge", "dilluns", "dimarts", "dimecres", "dijous", "divendres", "dissabte" };
static const char *MONTHB[12] = { "de gener", "de febrer", "de març", "d'abril", "de maig", "de juny", "de juliol", "d'agost", "de setembre", "d'octubre", "de novembre", "de desembre" };
/* ───────────── widgets de l'escriptori ─────────────
   Els widgets són peces d'informació que els paquets (o el sistema) ofereixen; l'usuari tria quins es veuen i en quin
   ordre (pantalla «Personalitza l'inici»). La selecció es desa a system/config/escriptori_inici.txt, una clau per línia
   amb la forma «paquet.widget». Es col·loquen de dreta a esquerra en graella de 4 columnes (Widget.cw = 2 o 4). */
typedef struct { char key[64]; const char *name; int cw, ch, mod; void (*draw)(int, int, int, int); } WEnt;
#define MAXW 48
static WEnt wl[MAXW]; static int nwl;
static int homeRectY, homeRectH, homeHasClock;   /* zona dels widgets, per refrescar-la sola cada minut */
static char homeKeys[MAXW][64]; static int nHome, homeLoaded;
#define HOMECFG CFG "escriptori_inici.txt"
static void w_clock(int x, int y, int w, int h) {
  time_t now = time(NULL); struct tm *tm = localtime(&now); char t[16], d[100];
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  SetFont(fHuge, BLACK); DrawTextRect(x, y, w, h * 7 / 10, t, ALIGN_CENTER | VALIGN_MIDDLE);
  snprintf(d, sizeof(d), "%s, %d %s", WDAYB[tm->tm_wday], tm->tm_mday, MONTHB[tm->tm_mon]);
  SetFont(fText, BLACK); DrawTextRect(x, y + h * 7 / 10, w, h * 3 / 10, d, ALIGN_CENTER | VALIGN_MIDDLE);
}
#define w_label_value ui_lv
static void w_batt(int x, int y, int w, int h) {
  char t[48]; snprintf(t, sizeof(t), "%d%%%s", GetBatteryPower(), IsCharging() ? " · carregant" : "");
  w_label_value(x, y, w, h, "Bateria", t);
}
static void w_space(int x, int y, int w, int h) {
  struct statvfs sv; char t[48] = "—";
  if (statvfs(FLASHDIR, &sv) == 0) snprintf(t, sizeof(t), "%ld MB", (long)((double)sv.f_bavail * (double)sv.f_frsize / 1048576.0));
  w_label_value(x, y, w, h, "Espai lliure", t);
}
static void wl_add(const char *key, const char *name, int cw, int ch, int mod, void (*dr)(int, int, int, int)) {
  if (nwl >= MAXW) return;
  snprintf(wl[nwl].key, sizeof(wl[nwl].key), "%s", key); wl[nwl].name = name;
  wl[nwl].cw = cw == 4 ? 4 : 2; wl[nwl].ch = ch < 1 ? 1 : ch > 3 ? 3 : ch; wl[nwl].mod = mod; wl[nwl].draw = dr; nwl++;
}
static void wl_collect(void) {
  int i, j; char k[64];
  nwl = 0;
  wl_add("base.rellotge", "Rellotge i data", 4, 2, -1, w_clock);
  wl_add("base.bateria", "Bateria", 2, 1, -1, w_batt);
  wl_add("base.espai", "Espai lliure", 2, 1, -1, w_space);
  for (i = 0; i < nmods; i++)
    for (j = 0; j < mods[i].m->nwidgets; j++) {
      const Widget *w = &mods[i].m->widgets[j];
      snprintf(k, sizeof(k), "%s.%s", mods[i].id, w->id);
      wl_add(k, w->name, w->cw, w->ch, i, w->draw);
    }
}
static int wl_find(const char *key) { int i; for (i = 0; i < nwl; i++) if (!strcmp(wl[i].key, key)) return i; return -1; }
static int home_find(const char *key) { int i; for (i = 0; i < nHome; i++) if (!strcmp(homeKeys[i], key)) return i; return -1; }
static void home_add(const char *key) { if (nHome < MAXW && home_find(key) < 0) snprintf(homeKeys[nHome++], sizeof(homeKeys[0]), "%s", key); }
static void home_load(void) {
  struct stat st; FILE *f; char ln[100];
  if (homeLoaded) return;
  homeLoaded = 1; nHome = 0;
  if (stat(HOMECFG, &st) != 0) { home_add("base.rellotge"); home_add("temps.avui"); home_add("tasques.pendents"); home_add("agenda.proper"); return; }
  f = fopen(HOMECFG, "r");
  if (!f) return;
  while (fgets(ln, sizeof(ln), f)) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) home_add(ln); }
  fclose(f);
}
static void home_save(void) {
  FILE *f = fopen(HOMECFG, "w"); int i;
  if (!f) return;
  for (i = 0; i < nHome; i++) fprintf(f, "%s\n", homeKeys[i]);
  fclose(f);
}
/* Dibuixa els widgets escollits a partir de y0 sense passar de maxh; retorna l'alçada ocupada (amb la separació final). */
static int home_draw(int y0, int maxh) {
  int unit = 3 * U, gap = U / 3, W = SW - U, idx[MAXW], rowOf[MAXW], colOf[MAXW], rowH[MAXW + 1], rowY[MAXW + 1];
  int n = 0, i, row = 0, col = 0, nrows, used = 0, shown = 0;
  for (i = 0; i < nHome && n < MAXW; i++) { int k = wl_find(homeKeys[i]); if (k >= 0) idx[n++] = k; }
  memset(rowH, 0, sizeof(rowH));
  for (i = 0; i < n; i++) {
    const WEnt *e = &wl[idx[i]]; int h = e->ch * unit;
    if (col + e->cw > 4) { row++; col = 0; }
    rowOf[i] = row; colOf[i] = col; col += e->cw;
    if (h > rowH[row]) rowH[row] = h;
    if (col >= 4) { row++; col = 0; }
  }
  nrows = col > 0 ? row + 1 : row;
  homeHasClock = 0; homeRectY = y0; homeRectH = 0;
  for (i = 0; i < nrows; i++) {
    if (used + rowH[i] > maxh) break;
    rowY[i] = y0 + used; used += rowH[i] + gap; shown = i + 1; homeRectH = used - gap;
  }
  for (i = 0; i < n; i++) {
    const WEnt *e = &wl[idx[i]]; int r = rowOf[i], x, w, h;
    if (r >= shown) continue;
    h = rowH[r];
    if (e->cw == 4) { x = U / 2; w = W; }
    else { w = (W - gap) / 2; x = colOf[i] == 0 ? U / 2 : U / 2 + W - w; }
    if (!strcmp(e->key, "base.rellotge")) homeHasClock = 1;
    ui_card(x, rowY[r], w, h, UI_LINE);
    if (e->draw) e->draw(x + U / 2, rowY[r] + U / 3, w - U, h - 2 * (U / 3));
    add_hit(x, rowY[r], w, h, A_WIDGET, idx[i]);
  }
  return used;
}

static int pkgs_incompatible(void) { int n = pkgs_on_disk() - nmods; return n > 0 ? n : 0; }
static void v_desk(View *v) {
  char d[200]; int y = SB + U / 2, cols = 4, cw = (SW - U) / cols, s = cw * 55 / 100, cellh, per, i, total, k = 0;
  ext_scan(); wl_collect(); home_load();
  total = nmods + 2 + nExt;
  y += home_draw(y, (BOTTOM - y) * 55 / 100) + U / 4;
  cellh = s + 2 * U + U / 4;
  per = ((BOTTOM - y) / cellh) * cols; if (per < cols) per = cols;
  pages = (total + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < total && k < per; i++, k++) {
    int cx = U / 2 + (k % cols) * cw, cy = y + (k / cols) * cellh, ix = cx + (cw - s) / 2;
    ui_card(ix - U / 6, cy - U / 6, s + U / 3, s + U / 3, UI_SOFT);
    if (i < nmods) { if (mods[i].m->fn_icon) mods[i].m->fn_icon(ix, cy, s); snprintf(d, sizeof(d), "%s", mods[i].m->name); }
    else if (i == nmods) { icon_store(ix, cy, s); snprintf(d, sizeof(d), "Paquets"); }
    else if (i == nmods + 1) { icon_system(ix, cy, s); snprintf(d, sizeof(d), "Sistema"); }
    else {
      size_t n; char t[8];
      rfill(ix + s / 6, cy + s / 6, s * 2 / 3, s * 2 / 3, s / 8, BLACK);
      snprintf(d, sizeof(d), "%s", extApps[i - nmods - 2]); n = strlen(d); if (n > 4) d[n - 4] = 0;
      t[0] = d[0]; t[1] = 0; if ((unsigned char)d[0] >= 0xC0) { t[1] = d[1]; t[2] = 0; }
      SetFont(fHuge, WHITE); DrawTextRect(ix, cy, s, s, t, ALIGN_CENTER | VALIGN_MIDDLE);
    }
    SetFont(fSmall, BLACK); DrawTextRect(cx + 2, cy + s + U / 6, cw - 4, U + U / 2, d, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    add_hit(cx, cy, cw, s + 2 * U, A_APP, i);
  }
  if (!nmods && v->page == 0) {
    int old = pkgs_incompatible(); char t[260];
    if (old) snprintf(t, sizeof(t), "Hi ha %d paquets d'una versió anterior del sistema.\nObre «Paquets» i prem «Instal·la i actualitza-ho tot».", old);
    else snprintf(t, sizeof(t), "Encara no hi ha cap aplicació instal·lada.\nObre «Paquets» amb el Wi-Fi connectat per baixar-ne.");
    SetFont(fText, BLACK);
    DrawTextRect(U / 2, y + cellh, SW - U, 4 * U, t, ALIGN_CENTER | VALIGN_TOP);
  }
}

/* ───────────── personalitza l'inici ───────────── */
static void v_home(View *v) {
  int y = TOP, rh = 3 * U, per, i, n = 0, nen, ord[MAXW], tw = 2 * U, th = U + U / 2, bs = 2 * U;
  wl_collect(); home_load();
  for (i = 0; i < nHome; i++) { int k = wl_find(homeKeys[i]); if (k >= 0) ord[n++] = k; }
  nen = n;
  for (i = 0; i < nwl; i++) if (home_find(wl[i].key) < 0) ord[n++] = i;
  header("Personalitza l'inici", "Tria quins widgets es veuen i en quin ordre");
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = n ? (n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    int k = ord[i], en = i < nen, ry = y + (i - v->page * per) * rh, tx = SW - U - tw, by = ry + (rh - U / 4 - bs) / 2; char sub[80];
    ui_card(U / 2, ry, SW - U, rh - U / 4, en ? UI_LINE : UI_SOFT);
    snprintf(sub, sizeof(sub), "%s · %s", wl[k].cw == 4 ? "ample" : "mig", wl[k].mod >= 0 ? mods[wl[k].mod].m->name : "sistema");
    SetFont(fBold, en ? BLACK : DGRAY); DrawTextRect(U, ry + U / 8, tx - 2 * bs - 2 * U, rh / 2 - U / 8, wl[k].name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, DGRAY); DrawTextRect(U, ry + rh / 2 - U / 4, tx - 2 * bs - 2 * U, rh / 2 - U / 4, sub, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    ui_toggle(tx, ry + (rh - U / 4 - th) / 2, tw, th, en);
    add_hit(tx - U / 2, ry, tw + U, rh - U / 4, A_HE_TOGGLE, k);
    if (en) {
      int ux = tx - U - 2 * bs - U / 4, up = i > 0, dn = i < nen - 1, ah = bs * 5 / 10;
      rcard(ux, by, bs, bs, 8, 2, up ? BLACK : LGRAY, WHITE); ic_arrow(ux + bs / 2, by + (bs - ah) / 2, ah, 0, up ? BLACK : LGRAY);
      rcard(ux + bs + U / 4, by, bs, bs, 8, 2, dn ? BLACK : LGRAY, WHITE); ic_arrow(ux + bs + U / 4 + bs / 2, by + (bs - ah) / 2, ah, 1, dn ? BLACK : LGRAY);
      if (up) add_hit(ux, by, bs, bs, A_HE_UP, k);
      if (dn) add_hit(ux + bs + U / 4, by, bs, bs, A_HE_DN, k);
    }
  }
  if (!n) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 3 * U, "No hi ha cap widget disponible.", ALIGN_LEFT | VALIGN_TOP); }
}
static void home_move(int key, int dir) {
  int p = home_find(wl[key].key), q;
  if (p < 0) return;
  for (q = p + dir; q >= 0 && q < nHome && wl_find(homeKeys[q]) < 0; q += dir) {}
  if (q < 0 || q >= nHome) return;
  { char t[64]; memcpy(t, homeKeys[p], 64); memcpy(homeKeys[p], homeKeys[q], 64); memcpy(homeKeys[q], t, 64); }
  home_save();
}
static void home_toggle(int key) {
  int p = home_find(wl[key].key);
  if (p >= 0) { memmove(homeKeys[p], homeKeys[p + 1], sizeof(homeKeys[0]) * (size_t)(nHome - p - 1)); nHome--; }
  else home_add(wl[key].key);
  home_save();
}

/* ───────────── tauler ràpid ───────────── */
static int light_get(void) { int v = GetFrontlightState(); return v == INT_MIN ? -1 : v; }
static void v_quick(View *v) {
  int gap = U / 2, tw = (SW - U - gap) / 2, th = 5 * U, x0 = U / 2, x1 = U / 2 + tw + gap, y = TOP, net = QueryNetwork(), lv = light_get(), bs = 2 * U, r = U * 8 / 10;
  char t[100], st[60]; int solid;
  (void)v;
  snprintf(t, sizeof(t), "Bateria %d%%%s", GetBatteryPower(), IsCharging() ? " · carregant" : "");
  header("Tauler ràpid", t);
  /* fila 1: Wi-Fi i llum */
  solid = (net & NET_WIFIREADY) != 0;
  snprintf(st, sizeof(st), "%s", solid ? "connectat · toca per desconnectar" : (net & NET_WIFI) ? "encès · toca per connectar" : "apagat · toca per connectar");
  ui_tile(x0, y, tw, th, "Wi-Fi", st, solid, A_QWIFI, 0);
  ic_wifi(x0 + U + U / 2, y + U + U / 2, r, solid ? 3 : 0, solid ? WHITE : BLACK, solid ? WHITE : DGRAY, !solid && !(net & NET_WIFI));
  if (lv >= 0) snprintf(st, sizeof(st), "%d%%", lv); else snprintf(st, sizeof(st), "no disponible");
  ui_tile(x1, y, tw, th, "Llum", st, 0, 0, 0);
  ic_sun(x1 + U + U / 2, y + U + U / 2, r, BLACK);
  rcard(x1 + tw - U / 2 - 2 * bs - U / 3, y + U / 2, bs, bs, 8, 2, BLACK, WHITE); ic_minus(x1 + tw - U / 2 - 2 * bs - U / 3 + bs / 2, y + U / 2 + bs / 2, bs * 6 / 10, BLACK);
  rcard(x1 + tw - U / 2 - bs, y + U / 2, bs, bs, 8, 2, BLACK, WHITE); ic_plus(x1 + tw - U / 2 - bs / 2, y + U / 2 + bs / 2, bs * 6 / 10, BLACK);
  add_hit(x1 + tw - U / 2 - 2 * bs - U / 3, y + U / 2, bs, bs, A_QLIGHT_DN, 0);
  add_hit(x1 + tw - U / 2 - bs, y + U / 2, bs, bs, A_QLIGHT_UP, 0);
  /* fila 2: neteja de pantalla i inici */
  y += th + gap;
  ui_tile(x0, y, tw, th, "Neteja la pantalla", "refresc complet", 0, A_QCLEAN, 0);
  ic_refresh(x0 + U + U / 2, y + U + U / 2, r, BLACK);
  ui_tile(x1, y, tw, th, "Personalitza l'inici", "widgets i ordre", 0, A_QGO, 2);
  ic_grid(x1 + U + U / 2, y + U + U / 2, r * 8 / 10, BLACK);
  /* fila 3: paquets i sistema */
  y += th + gap;
  snprintf(st, sizeof(st), "%d instal·lats", nmods);
  ui_tile(x0, y, tw, th, "Paquets", st, 0, A_QGO, 1);
  icon_store(x0 + U / 2, y + U / 4, 2 * U + U / 2);
  snprintf(st, sizeof(st), "versió %s", APP_VERNAME);
  ui_tile(x1, y, tw, th, "Sistema", st, 0, A_QGO, 0);
  icon_system(x1 + U / 2, y + U / 4, 2 * U + U / 2);
}
static void v_store(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 3 * U, per, i, bw = (SW - U - U / 3) / 2, inst = 0, pend = 0; char sub[100];
  for (i = 0; i < ncat; i++) { int pv = pkg_ver(cat[i].id); if (pv) inst++; if (!pv || pv < cat[i].ver || mod_find(cat[i].id) < 0) pend++; }
  snprintf(sub, sizeof(sub), "%d al catàleg · %d instal·lats", ncat, inst);
  header("Paquets", sub);
  button(U / 2, y, bw, bh, "Actualitza el catàleg", 0, A_STORE_REFRESH, 0);
  if (pend) button(U / 2 + bw + U / 3, y, bw, bh, inst ? "Instal·la i actualitza-ho tot" : "Instal·la-ho tot", 0, A_STORE_ALL, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = ncat ? (ncat + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!ncat) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 5 * U, "Encara no s'ha baixat el catàleg.\nConnecta el Wi-Fi i prem «Actualitza el catàleg».", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < ncat && i < (v->page + 1) * per; i++) {
    ui_row(U / 2, y, SW - U, rh - U / 4, cat[i].name, cat[i].desc, pkg_state(&cat[i]), A_PKG, i);
    y += rh;
  }
}
static void v_pkg(View *v) {
  const Pkg *q; char t[400], sz[32]; int y = TOP, bh = 2 * U + U / 2, pv;
  if (v->a < 0 || v->a >= ncat) { header("Paquet", ""); return; }
  q = &cat[v->a]; pv = pkg_ver(q->id);
  header(q->name, pkg_state(q));
  SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 4 * U, q->desc, ALIGN_LEFT | VALIGN_TOP); y += 4 * U + U / 2;
  fmt_size(sz, sizeof(sz), q->size);
  if (pv) snprintf(t, sizeof(t), "Versió del catàleg: %d · instal·lada: %d · mida: %s", q->ver, pv, sz);
  else snprintf(t, sizeof(t), "Versió: %d · mida: %s", q->ver, sz);
  SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS); y += 2 * U + U / 2;
  if (!pv) button(U / 2, y, SW - U, bh, "Instal·la", 1, A_PKG_INSTALL, v->a);
  else {
    if (pv < q->ver || mod_find(q->id) < 0) { button(U / 2, y, SW - U, bh, "Actualitza", 1, A_PKG_INSTALL, v->a); y += bh + U / 3; }
    button(U / 2, y, SW - U, bh, "Desinstal·la", 0, A_PKG_REMOVE, v->a);
  }
}
static void v_system(View *v) {
  char t[500], b[32]; int y = TOP, bh = 2 * U + U / 2, lh = U + U * 3 / 4, net = QueryNetwork(); struct statvfs sv;
  (void)v;
  snprintf(t, sizeof(t), "Escriptori %s (compilació %d)", APP_VERNAME, APP_BUILD);
  header("Sistema", t);
  SetFont(fText, BLACK);
  snprintf(t, sizeof(t), "Bateria: %d%%%s", GetBatteryPower(), IsCharging() ? " (carregant)" : "");
  DrawTextRect(U / 2, y, SW - U, lh, t, ALIGN_LEFT | VALIGN_MIDDLE); y += lh;
  snprintf(t, sizeof(t), "Wi-Fi: %s", (net & NET_WIFIREADY) ? "connectat" : (net & NET_WIFI) ? "encès, sense connexió" : "apagat");
  DrawTextRect(U / 2, y, SW - U, lh, t, ALIGN_LEFT | VALIGN_MIDDLE); y += lh;
  if (statvfs(FLASHDIR, &sv) == 0) {
    snprintf(b, sizeof(b), "%ld MB", (long)((double)sv.f_bavail * (double)sv.f_frsize / 1048576.0));
    snprintf(t, sizeof(t), "Espai lliure: %s", b);
    DrawTextRect(U / 2, y, SW - U, lh, t, ALIGN_LEFT | VALIGN_MIDDLE); y += lh;
  }
  snprintf(t, sizeof(t), "Paquets carregats: %d", nmods);
  DrawTextRect(U / 2, y, SW - U, lh, t, ALIGN_LEFT | VALIGN_MIDDLE); y += lh + U / 2;
  if (!(net & NET_WIFIREADY)) { button(U / 2, y, SW - U, bh, "Connecta el Wi-Fi", 0, A_WIFI, 0); y += bh + U / 2; }
  FillArea(U / 2, y, SW - U, 2, BLACK); y += U / 2;
  SetFont(fBold, BLACK); DrawTextRect(U / 2, y, SW - U, lh, "Actualització del sistema", ALIGN_LEFT | VALIGN_MIDDLE); y += lh;
  if (updState == 0) snprintf(t, sizeof(t), "Comprova si hi ha una versió nova de l'Escriptori.");
  else if (updState == 1) snprintf(t, sizeof(t), "Ja tens l'última versió (el servidor té la compilació %d).", updBuild);
  else if (updState == 2) snprintf(t, sizeof(t), "Hi ha la versió %s disponible (compilació %d).%s%s", updName, updBuild, updNotes[0] ? "\n" : "", updNotes);
  else if (updState == 3) snprintf(t, sizeof(t), "S'ha instal·lat la versió %s.\nTanca l'Escriptori amb la X i torna'l a obrir.", updName);
  else snprintf(t, sizeof(t), "No s'ha pogut completar. Torna-ho a provar amb el Wi-Fi connectat.");
  SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 5 * U, t, ALIGN_LEFT | VALIGN_TOP); y += 5 * U + U / 2;
  if (updState == 2) button(U / 2, y, SW - U, bh, "Descarrega i instal·la", 1, A_UPD_INSTALL, 0);
  else if (updState != 3) button(U / 2, y, SW - U, bh, "Comprova si hi ha una versió nova", 0, A_UPD_CHECK, 0);
  SetFont(fSmall, BLACK);
  DrawTextRect(U / 2, BOTTOM - U, SW - U, 2 * U, "Sistema i paquets: github.com/sarnaloka/pb_launcher", ALIGN_LEFT | VALIGN_MIDDLE);
}

static int tick_redraw(void) {
  View *v;
  if (depth < 1) return 0;
  v = &stack[depth - 1];
  if (v->mod >= 0) return v->mod < nmods && (mods[v->mod].m->flags & MOD_TICK);
  return v->type == V_DESK;
}
static void render(void) {
  View *v = &stack[depth - 1];
  int pass;
  netAct = 0;                              /* qualsevol redibuixat completa l'indicador de xarxa */
  if (v->mod >= nmods) { depth = 1; v = &stack[0]; }
  for (pass = 0; pass < 2; pass++) {
    int before = v->page;
    ClearScreen(); nhits = 0; pages = 1;
    statusbar(1);
    if (v->mod >= 0) mods[v->mod].m->fn_draw(v);
    else switch (v->type) {
      case V_DESK: v_desk(v); break;
      case V_STORE: v_store(v); break;
      case V_PKG: v_pkg(v); break;
      case V_SYSTEM: v_system(v); break;
      case V_QUICK: v_quick(v); break;
      case V_HOME: v_home(v); break;
    }
    if (v->page < 0) v->page = 0;
    footer(v->page);
    if (v->page == before) break;
  }
}
static void draw(int full) {
  render();
  if (full) FullUpdate(); else SoftUpdate();
  if (pendMsg) { const char *m = pendMsg; pendMsg = NULL; Message(ICON_INFORMATION, "Escriptori", m, 4000); }
}
/* Tic d'un minut a l'escriptori: només es refresca la barra d'estat i, si n'hi ha, la zona del rellotge (no tota la pantalla). */
static int desk_tick(void) {
  View *v = &stack[depth - 1];
  if (v->mod >= 0 || v->type != V_DESK) return 0;
  render();
  PartialUpdate(0, 0, SW, SB);
  if (homeHasClock && homeRectH > 0) PartialUpdate(0, homeRectY, SW, homeRectH);
  return 1;
}
static View *push_view(int mod, int type, int a, int b, int sel, const char *term) {
  View *v;
  if (depth >= MAXSTACK) { memmove(&stack[1], &stack[2], sizeof(View) * (MAXSTACK - 2)); depth--; }
  v = &stack[depth++];
  memset(v, 0, sizeof(*v));
  v->mod = mod; v->type = type; v->a = a; v->b = b; v->sel = sel;
  if (term) strncpy(v->term, term, sizeof(v->term) - 1);
  return v;
}
static void h_push(int type, int a, int b, int sel, const char *term) { push_view(stack[depth - 1].mod, type, a, b, sel, term); draw(1); }
static void h_pop(void) { if (depth > 1) depth--; }

static void open_module(int i) {
  View *nv;
  if (i < 0 || i >= nmods) return;
  nv = push_view(i, 0, 0, 0, 0, NULL);
  if (mods[i].m->fn_open) mods[i].m->fn_open(nv);
}
static void act(int a, int arg) {
  View *v = &stack[depth - 1];
  static char msg[200];
  if (a >= 100) { if (v->mod >= 0 && v->mod < nmods && mods[v->mod].m->fn_action) mods[v->mod].m->fn_action(v, a, arg); return; }
  switch (a) {
    case A_BACK:
      if (v->mod >= 0 && mods[v->mod].m->fn_back && mods[v->mod].m->fn_back(v)) draw(1);
      else if (depth > 1) { depth--; draw(1); }
      break;
    case A_HOME: depth = 1; stack[0].page = 0; draw(1); break;
    case A_PREV: if (v->page > 0) { v->page--; draw(0); } break;
    case A_NEXT: if (v->page < pages - 1) { v->page++; draw(0); } break;
    case A_EXIT: CloseApp(); break;
    case A_APP:
      if (arg < 0) break;
      if (arg < nmods) { open_module(arg); draw(1); }
      else if (arg == nmods) { push_view(-1, V_STORE, 0, 0, 0, NULL); draw(1); }
      else if (arg == nmods + 1) { push_view(-1, V_SYSTEM, 0, 0, 0, NULL); draw(1); }
      else ext_launch(arg - nmods - 2);
      break;
    case A_STORE_REFRESH: { int rc = store_refresh(); if (rc) say(rc == -1 ? "No s'ha pogut baixar el catàleg. Comprova el Wi-Fi." : "El catàleg del servidor no és vàlid."); v->page = 0; draw(1); } break;
    case A_STORE_ALL: {
      int i, ok = 0, bad = 0;
      for (i = 0; i < ncat; i++) { int pv = pkg_ver(cat[i].id); if (pv && pv >= cat[i].ver && mod_find(cat[i].id) >= 0) continue; if (pkg_install(&cat[i]) == 0) ok++; else bad++; }
      if (bad) snprintf(msg, sizeof(msg), "Instal·lats: %d. Amb error: %d. Torna-ho a provar per als que falten.", ok, bad);
      else snprintf(msg, sizeof(msg), ok == 1 ? "S'ha instal·lat %d paquet." : "S'han instal·lat %d paquets.", ok);
      say(msg); draw(1);
    } break;
    case A_PKG: push_view(-1, V_PKG, arg, 0, 0, NULL); draw(1); break;
    case A_PKG_INSTALL: if (arg >= 0 && arg < ncat) { int rc = pkg_install(&cat[arg]); if (rc) say(pkg_err(rc)); draw(1); } break;
    case A_PKG_REMOVE: pkgSel = arg; confirm("Desinstal·la", "Es traurà el paquet del lector. Les teves dades es conserven.", "Desinstal·la", pkg_remove); break;
    case A_UPD_CHECK: upd_check(); draw(1); break;
    case A_UPD_INSTALL: upd_install(); draw(1); break;
    case A_WIFI: NetConnect(NULL); draw(1); break;
    case A_QUICK: if (!(v->mod < 0 && v->type == V_QUICK)) { push_view(-1, V_QUICK, 0, 0, 0, NULL); draw(1); } break;
    case A_WIDGET:
      if (arg < 0 || arg >= nwl) break;
      if (wl[arg].mod >= 0) open_module(wl[arg].mod);
      else if (!strcmp(wl[arg].key, "base.rellotge") && mod_find("rellotge") >= 0) open_module(mod_find("rellotge"));
      else push_view(-1, V_SYSTEM, 0, 0, 0, NULL);
      draw(1);
      break;
    case A_QWIFI: if (QueryNetwork() & NET_WIFIREADY) NetDisconnect(); else NetConnect(NULL); draw(0); break;
    case A_QLIGHT_DN: case A_QLIGHT_UP: {
      int cur = light_get(), nv;
      if (cur < 0) { say("Aquest lector no permet regular la llum des d'aquí."); draw(0); break; }
      nv = cur + (a == A_QLIGHT_UP ? 10 : -10); if (nv < 0) nv = 0; if (nv > 100) nv = 100;
      if (nv > 0) SetFrontlightEnabled(1);
      SetFrontlightState(nv);
      if (nv == 0) SetFrontlightEnabled(0);
      draw(0);
    } break;
    case A_QCLEAN: draw(1); break;
    case A_QGO: depth--; push_view(-1, arg == 1 ? V_STORE : arg == 2 ? V_HOME : V_SYSTEM, 0, 0, 0, NULL); draw(1); break;
    case A_HE_TOGGLE: if (arg >= 0 && arg < nwl) { home_toggle(arg); draw(0); } break;
    case A_HE_UP: if (arg >= 0 && arg < nwl) { home_move(arg, -1); draw(0); } break;
    case A_HE_DN: if (arg >= 0 && arg < nwl) { home_move(arg, 1); draw(0); } break;
  }
}

static int handler(int type, int par1, int par2) {
  int i;
  switch (type) {
    case EVT_INIT:
      SW = ScreenWidth(); SH = ScreenHeight(); U = SW / 27; if (U < 16) U = 16; SB = U * 9 / 4;
      SetPanelType(PANEL_DISABLED);
      SH -= PanelHeight();
      fTitle = OpenFont(DEFAULTFONTB, U * 11 / 10, 1); fBold = OpenFont(DEFAULTFONTB, U * 9 / 10, 1);
      fText = OpenFont(DEFAULTFONT, U * 9 / 10, 1); fSmall = OpenFont(DEFAULTFONT, U * 7 / 10, 1); fBig = OpenFont(DEFAULTFONTB, U * 12 / 10, 1); fHuge = OpenFont(DEFAULTFONTB, U * 3, 1);
      host.api = ESC_API; host.SW = SW; host.SH = SH; host.U = U; host.SB = SB;
      host.fTitle = fTitle; host.fBold = fBold; host.fText = fText; host.fSmall = fSmall; host.fBig = fBig; host.fHuge = fHuge;
      host.pages = &pages; host.add_hit = add_hit; host.button = button; host.header = header; host.redraw = draw; host.push = h_push; host.pop = h_pop;
      host.say = say; host.ask = ask; host.confirm = confirm; host.net_get = net_get_t;
      depth = 1; memset(&stack[0], 0, sizeof(View)); stack[0].type = V_DESK; stack[0].mod = -1;
      mkdir(SYSDIR, 0777); mkdir(PKGDIR, 0777);
      mods_scan(); cat_load();
      break;
    case EVT_SHOW:
    case EVT_FOREGROUND:
      draw(1);
      ClearTimer(clock_tick);
      SetWeakTimer("escriptori_clock", clock_tick, (int)(61 - time(NULL) % 60) * 1000);
      break;
    case EVT_BACKGROUND:
      ClearTimer(clock_tick);
      break;
    case EVT_POINTERUP:
      for (i = nhits - 1; i >= 0; i--)
        if (par1 >= hits[i].x && par1 < hits[i].x + hits[i].w && par2 >= hits[i].y && par2 < hits[i].y + hits[i].h) { act(hits[i].act, hits[i].arg); break; }
      break;
    case EVT_KEYPRESS:
      if (par1 == IV_KEY_PREV) act(A_PREV, 0);
      else if (par1 == IV_KEY_NEXT) act(A_NEXT, 0);
      else if (par1 == IV_KEY_BACK) { if (depth > 1) act(A_BACK, 0); else CloseApp(); }
      else return 0;
      return 1;
    case EVT_EXIT:
      ClearTimer(clock_tick);
      break;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc > 0 && argv[0]) { const char *b = strrchr(argv[0], '/'); snprintf(selfName, sizeof(selfName), "%s", b ? b + 1 : argv[0]); if (argv[0][0] == '/') snprintf(selfPath, sizeof(selfPath), "%s", argv[0]); }
  if (!selfPath[0]) snprintf(selfPath, sizeof(selfPath), APPDIR "/%s", selfName);
  srand((unsigned)time(NULL));
  InkViewMain(handler);
  return 0;
}
