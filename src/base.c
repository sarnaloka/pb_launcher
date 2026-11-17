/* Escriptori — sistema base: escriptori, barra d'estat, gestor de paquets i actualització. */
#include "mod.h"
#include <dlfcn.h>
#include <sys/statvfs.h>
#include <limits.h>

#define APP_BUILD 9
#define APP_VERNAME "2.2"
#define UPD_BASE "https://raw.githubusercontent.com/sarnaloka/pb_launcher/main/"
#define SYSDIR FLASHDIR "/system/escriptori"
#define PKGDIR SYSDIR "/packages"

enum { V_DESK, V_STORE, V_PKG, V_SYSTEM, V_QUICK, V_HOME, V_AVISOS, V_UPDATE, V_ABOUT, V_FAVS, V_LOOK, V_FONTS };   /* V_SYSTEM = Configuració */
enum { A_NONE, A_BACK, A_HOME, A_PREV, A_NEXT, A_EXIT, A_APP, A_STORE_REFRESH, A_STORE_ALL, A_PKG, A_PKG_INSTALL, A_PKG_REMOVE,
       A_UPD_CHECK, A_UPD_INSTALL, A_WIFI,
       A_QUICK, A_WIDGET, A_QWIFI, A_QLIGHT_DN, A_QLIGHT_UP, A_QCLEAN, A_QGO, A_HE_TOGGLE, A_HE_UP, A_HE_DN, A_AVISOS, A_AV_ONE, A_AV_READ, A_AV_CLEAR,
       A_CFG_GO, A_CFG_FULL, A_FAV_TOGGLE, A_STORE_FILTER, A_ABOUT_UPD,
       A_LOOK_THEME, A_LOOK_SIZE, A_LOOK_BG, A_LOOK_CON, A_FONT_PICK, A_DESK_PREV, A_DESK_NEXT };          /* les accions dels paquets comencen a 100 */
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

/* ───────────── aparença: ajustos, tema, fonts i fons ─────────────
   system/config/escriptori_config.txt: refresc, tema (0 clar, 1 fosc), mida (0..3), fons (0 cap, 1 gris, 2 imatge),
   contrast (0 suau, 1 fort) i font (família; buit = la del lector). */
static int cfgFull = 1, cfgLoaded, cfgTheme, cfgSize = 1, cfgBg = 1, cfgCon;
static char cfgFont[80];
static void cfg_load(void) {
  FILE *f; char ln[160];
  if (cfgLoaded) return;
  cfgLoaded = 1;
  f = fopen(CFG "escriptori_config.txt", "r");
  if (!f) return;
  while (fgets(ln, sizeof(ln), f)) {
    ln[strcspn(ln, "\r\n")] = 0;
    if (!strncmp(ln, "refresc=", 8)) { cfgFull = atoi(ln + 8); if (cfgFull < 1 || cfgFull > 20) cfgFull = 1; }
    else if (!strncmp(ln, "tema=", 5)) cfgTheme = atoi(ln + 5) == 1;
    else if (!strncmp(ln, "mida=", 5)) { cfgSize = atoi(ln + 5); if (cfgSize < 0 || cfgSize > 3) cfgSize = 1; }
    else if (!strncmp(ln, "fons=", 5)) { cfgBg = atoi(ln + 5); if (cfgBg < 0 || cfgBg > 2) cfgBg = 1; }
    else if (!strncmp(ln, "contrast=", 9)) cfgCon = atoi(ln + 9) == 1;
    else if (!strncmp(ln, "font=", 5)) snprintf(cfgFont, sizeof(cfgFont), "%s", ln + 5);
  }
  fclose(f);
}
static void cfg_save(void) {
  FILE *f = fopen(CFG "escriptori_config.txt", "w");
  if (!f) return;
  fprintf(f, "refresc=%d\ntema=%d\nmida=%d\nfons=%d\ncontrast=%d\nfont=%s\n", cfgFull, cfgTheme, cfgSize, cfgBg, cfgCon, cfgFont);
  fclose(f);
}
/* Paleta del tema (valors que es veuen). En fosc es desa invertida a escPal i la pantalla s'inverteix en acabar de
   dibuixar (render, sb_refresh, upd_region): així també s'inverteixen el BLACK i el WHITE que fan servir els paquets. */
static void theme_apply(void) {
  static const int L[2][P_N] = {
    { 0xE5E5E5, 0xFFFFFF, 0xC8C8C8, 0x111111, 0x666666, 0xEEEEEE, 0xDDDDDD, 0x111111, 0xFFFFFF },
    { 0xDDDDDD, 0xFFFFFF, 0x999999, 0x000000, 0x333333, 0xE5E5E5, 0xAAAAAA, 0x000000, 0xFFFFFF } };
  static const int D[2][P_N] = {
    { 0x0E0E0E, 0x262626, 0x0E0E0E, 0xF2F2F2, 0xAAAAAA, 0x3A3A3A, 0x333333, 0xF2F2F2, 0x0E0E0E },
    { 0x000000, 0x2C2C2C, 0x000000, 0xFFFFFF, 0xDDDDDD, 0x444444, 0x666666, 0xFFFFFF, 0x000000 } };
  int i;
  cfg_load();
  escDark = cfgTheme == 1;
  for (i = 0; i < P_N; i++) { int c = escDark ? D[cfgCon][i] : L[cfgCon][i]; escPal[i] = escDark ? (0xFFFFFF ^ c) : c; }
  if (cfgBg == 0) {                                   /* sense fons: blanc (o negre en fosc), targetes amb vora o més clares */
    escPal[P_BG] = 0xFFFFFF;
    if (escDark) escPal[P_SHADOW] = 0xFFFFFF;
  }
  ICON_FG = PAL(P_FG); ICON_BG = PAL(P_CARD);
}
/* Fonts: la família triada (regular i, si n'hi ha, negreta) a quatre mides. EnumFonts dona els noms dels fitxers
   instal·lats al lector; les que posis a la carpeta «Fonts» de la memòria es copien a system/fonts. */
#define FONTSDIR FLASHDIR "/Fonts"
static ifont *fHead, *fClock;
static int font_has(char **L, const char *n) { for (; L && *L; L++) if (!strcmp(*L, n)) return 1; return 0; }
static void font_names(char **L, const char *fam, char *reg, char *bold, int sz) {
  static const char *BS[] = { "-Bold", "-SemiBold", "-Semibold", "-Medium", NULL };
  char t[140]; int i;
  if (!fam || !fam[0]) { snprintf(reg, (size_t)sz, "%s", DEFAULTFONT); snprintf(bold, (size_t)sz, "%s", DEFAULTFONTB); return; }
  if (font_has(L, fam)) snprintf(reg, (size_t)sz, "%s", fam);
  else { snprintf(t, sizeof(t), "%s-Regular", fam); snprintf(reg, (size_t)sz, "%s", font_has(L, t) ? t : DEFAULTFONT); }
  snprintf(bold, (size_t)sz, "%s", reg);
  for (i = 0; BS[i]; i++) { snprintf(t, sizeof(t), "%s%s", fam, BS[i]); if (font_has(L, t)) { snprintf(bold, (size_t)sz, "%s", t); break; } }
}
static ifont *font_open(const char *name, const char *fallback, int size) { ifont *f = OpenFont(name, size, 1); return f ? f : OpenFont(fallback, size, 1); }
static void fonts_apply(void) {
  static const int K[4] = { 88, 100, 112, 126 };
  ifont *old[8]; char r[140], b[140]; int k, i;
  cfg_load(); k = K[cfgSize];
  old[0] = fTitle; old[1] = fBold; old[2] = fText; old[3] = fSmall; old[4] = fBig; old[5] = fHuge; old[6] = fHead; old[7] = fClock;
  font_names(cfgFont[0] ? EnumFonts() : NULL, cfgFont, r, b, sizeof(r));
  fTitle = font_open(b, DEFAULTFONTB, U * 11 * k / 1000); fBold = font_open(b, DEFAULTFONTB, U * 9 * k / 1000);
  fText = font_open(r, DEFAULTFONT, U * 9 * k / 1000); fSmall = font_open(r, DEFAULTFONT, U * 7 * k / 1000);
  fBig = font_open(b, DEFAULTFONTB, U * 12 * k / 1000); fHuge = font_open(b, DEFAULTFONTB, U * 3);
  fHead = font_open(b, DEFAULTFONTB, U * 14 * (100 + (k - 100) / 2) / 1000); fClock = font_open(b, DEFAULTFONTB, U * 9 / 2);
  for (i = 0; i < 8; i++) if (old[i]) CloseFont(old[i]);
}
static void esc_copy_file(const char *a, const char *b) {
  FILE *f = fopen(a, "rb"), *g; char buf[16384]; size_t n;
  if (!f) return;
  g = fopen(b, "wb");
  if (!g) { fclose(f); return; }
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) fwrite(buf, 1, n, g);
  fclose(f); fclose(g);
}
static int is_font_file(const char *n) { const char *d = strrchr(n, '.'); return d && (!strcasecmp(d, ".ttf") || !strcasecmp(d, ".otf")); }
/* Copia a system/fonts les fonts noves de la carpeta «Fonts» (la crea si no hi és, perquè es vegi per FTP). */
static void fonts_import(void) {
  DIR *d; struct dirent *e; char a[600], b[600]; struct stat st;
  mkdir(FONTSDIR, 0777); mkdir(USERFONTDIR, 0777);
  if (!(d = opendir(FONTSDIR))) return;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.' || !is_font_file(e->d_name)) continue;
    snprintf(a, sizeof(a), "%s/%s", FONTSDIR, e->d_name); snprintf(b, sizeof(b), "%s/%s", USERFONTDIR, e->d_name);
    if (stat(b, &st) != 0) esc_copy_file(a, b);
  }
  closedir(d);
}
#define MAXFAM 80
static char famN[MAXFAM][64]; static int nfam, famUser[MAXFAM];
static int style_word(const char *w) {
  static const char *S[] = { "Regular", "Bold", "Italic", "BoldItalic", "Oblique", "BoldOblique", "Light", "LightItalic", "Medium", "MediumItalic",
                             "SemiBold", "Semibold", "SemiBoldItalic", "Black", "Thin", "ExtraLight", "ExtraBold", "Book", "Condensed", NULL };
  int i; for (i = 0; S[i]; i++) if (!strcmp(w, S[i])) return 1; return 0;
}
static int fam_cmp(const void *a, const void *b) { return strcasecmp((const char *)a, (const char *)b); }
static void fam_scan(void) {
  char **L, **q, t[80], *dash, rr[80]; int i, j; DIR *d; struct dirent *e;
  fonts_import();
  L = EnumFonts(); nfam = 0;
  for (q = L; q && *q && nfam < MAXFAM; q++) {
    snprintf(t, sizeof(t), "%s", *q); dash = strrchr(t, '-');
    if (dash && style_word(dash + 1)) *dash = 0;
    for (i = 0; i < nfam && strcmp(famN[i], t); i++) {}
    if (i < nfam) continue;
    snprintf(rr, sizeof(rr), "%s-Regular", t);
    if (!font_has(L, t) && !font_has(L, rr)) continue;           /* sense versió normal (només cursiva, etc.) */
    snprintf(famN[nfam++], 64, "%s", t);
  }
  qsort(famN, (size_t)nfam, sizeof(famN[0]), fam_cmp);
  for (i = 0; i < nfam; i++) famUser[i] = 0;
  if ((d = opendir(FONTSDIR))) {
    while ((e = readdir(d))) if (is_font_file(e->d_name)) for (j = 0; j < nfam; j++) if (!strncmp(e->d_name, famN[j], strlen(famN[j]))) famUser[j] = 1;
    closedir(d);
  }
}
/* Fons de l'inici amb imatge: la primera .jpg/.jpeg/.png de la carpeta «Fons». */
#define WALLDIR FLASHDIR "/Fons"
static ibitmap *wallBmp; static char wallPath[600];
static void wall_find(char *out, int sz) {
  DIR *d; struct dirent *e; char best[300] = "";
  out[0] = 0;
  if (!(d = opendir(WALLDIR))) return;
  while ((e = readdir(d))) {
    const char *x = strrchr(e->d_name, '.');
    if (e->d_name[0] == '.' || !x || (strcasecmp(x, ".jpg") && strcasecmp(x, ".jpeg") && strcasecmp(x, ".png"))) continue;
    if (!best[0] || strcmp(e->d_name, best) < 0) snprintf(best, sizeof(best), "%s", e->d_name);
  }
  closedir(d);
  if (best[0]) snprintf(out, (size_t)sz, "%s/%s", WALLDIR, best);
}
/* Dibuixa la imatge de fons sota la barra d'estat; retorna 1 si n'hi ha. */
static int wall_draw(void) {
  char p[600]; int x, y;
  wall_find(p, sizeof(p));
  if (!p[0]) return 0;
  if (!wallBmp || strcmp(p, wallPath)) {
    const char *x2 = strrchr(p, '.');
    free(wallBmp); wallBmp = NULL;
    snprintf(wallPath, sizeof(wallPath), "%s", p);
    wallBmp = (x2 && !strcasecmp(x2, ".png")) ? LoadPNGStretch(p, SW, SH - SB, 1, 0) : LoadJPEG(p, SW, SH - SB, 64, 128, 1);
  }
  if (!wallBmp) return 0;
  x = (SW - wallBmp->width) / 2; y = SB + (SH - SB - wallBmp->height) / 2;
  DrawBitmap(x, y, wallBmp);
  if (escDark) InvertArea(x, y, wallBmp->width, wallBmp->height);    /* la inversió final la deixarà com és */
  return 1;
}
static int wallOn;                                    /* l'inici d'ara té imatge de fons: els textos solts porten fons */
/* Text sobre el fons de l'inici: si hi ha imatge, amb una píndola del color de fons al darrere. */
static void desk_text(int x, int y, int w, int h, const char *t, ifont *f, int col, int align) {
  SetFont(f, col);
  if (wallOn) {
    int tw = StringWidth(t) + U, bx = align & ALIGN_CENTER ? x + (w - tw) / 2 : align & ALIGN_RIGHT ? x + w - tw : x - U / 2;
    if (tw > w + U) { tw = w + U; bx = x - U / 2; }
    rfill(bx, y, tw, h, h / 2, PAL(P_BG));
  }
  DrawTextRect(x, y, w, h, t, align | VALIGN_MIDDLE | DOTS);
}
static void seg(int x, int y, int w, int h, const char **lab, int n, int cur, int act);
static int sb_on_desk(void) { return depth >= 1 && stack[depth - 1].mod < 0 && stack[depth - 1].type == V_DESK; }
/* Actualització parcial (també per als paquets): amb el tema fosc s'inverteix abans la zona redibuixada. */
static void upd_region(int x, int y, int w, int h) { if (escDark) InvertArea(x, y, w, h); PartialUpdate(x, y, w, h); }

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
  if (!m || m->api < ESC_API_MIN || m->api > ESC_API || !m->fn_draw || !m->name) { dlclose(dl); return -2; }
  drop_module_views();
  for (i = nmods; i > 0 && strcasecmp(mods[i - 1].m->name, m->name) > 0; i--) mods[i] = mods[i - 1];
  mods[i].dl = dl; mods[i].m = m; snprintf(mods[i].id, sizeof(mods[i].id), "%s", id);
  nmods++;
  return 0;
}
/* Funcions de neteja que els paquets registren amb Host.on_unload: es criden abans de descarregar-los. */
static struct { char id[48]; void (*fn)(void); } unl[16]; static int nunl;
static void h_on_unload(const char *id, void (*fn)(void)) {
  int k;
  if (!id || !fn) return;
  for (k = 0; k < nunl; k++) if (!strcmp(unl[k].id, id)) { unl[k].fn = fn; return; }
  if (nunl < 16) { snprintf(unl[nunl].id, sizeof(unl[nunl].id), "%s", id); unl[nunl].fn = fn; nunl++; }
}
static void mod_unload(int i) {
  int k;
  if (i < 0 || i >= nmods) return;
  drop_module_views();
  for (k = 0; k < nunl; k++) if (!strcmp(unl[k].id, mods[i].id)) { unl[k].fn(); unl[k] = unl[nunl - 1]; nunl--; break; }
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
static void pkg_notify(void);
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
  pkg_notify();
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
static void avisos_add(const char *src, const char *title, const char *text);
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
  if (updState == 2) { char t[120]; snprintf(t, sizeof(t), "Versió %s", updName); avisos_add("Sistema", "Actualització disponible", t); }
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
static void icon_store(int x, int y, int s) { ic_path(IC_PACKAGE, x, y, s, 1.8, ICON_FG, ICON_BG); }
static void icon_system(int x, int y, int s) { ic_path(IC_SLIDERS, x, y, s, 1.8, ICON_FG, ICON_BG); }
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
static void w_clock(int x, int y, int w, int h) {   /* rellotge gran; a sota, salutació i data */
  time_t now = time(NULL); struct tm *tm = localtime(&now); char t[16], d[140];
  const char *hi = tm->tm_hour < 6 ? "Bona nit" : tm->tm_hour < 13 ? "Bon dia" : tm->tm_hour < 20 ? "Bona tarda" : "Bona nit";
  int ch = h - U * 5 / 4;
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  if (wallOn) ui_card(x - U / 4, y, w + U / 2, h, UI_LINE);
  SetFont(fClock ? fClock : fHuge, PAL(P_FG)); DrawTextRect(x + U / 4, y, w - U / 2, ch, t, ALIGN_LEFT | VALIGN_BOTTOM);
  snprintf(d, sizeof(d), "%s · %s, %d %s", hi, WDAYB[tm->tm_wday], tm->tm_mday, MONTHB[tm->tm_mon]);
  SetFont(fText, PAL(P_SUB)); DrawTextRect(x + U / 4, y + ch, w - U / 2, U + U / 4, d, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
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
    if (!strcmp(e->key, "base.rellotge")) { if (e->draw) e->draw(x + U / 4, rowY[r], w - U / 2, h); }
    else { ui_card(x, rowY[r], w, h, UI_LINE); if (e->draw) e->draw(x + U / 2 + U / 8, rowY[r] + U / 3, w - U - U / 4, h - 2 * (U / 3)); }
    add_hit(x, rowY[r], w, h, A_WIDGET, idx[i]);
  }
  return used;
}

static int pkgs_incompatible(void) { int n = pkgs_on_disk() - nmods; return n > 0 ? n : 0; }
/* Preferits: fins a 4 aplicacions a la barra de baix de l'inici. Clau = id del paquet, «@paquets» o «@config». */
#define FAVCFG CFG "escriptori_preferits.txt"
static char favKeys[4][64]; static int nFav, favLoaded;
static void fav_load(void) {
  FILE *f; char ln[100];
  if (favLoaded) return;
  favLoaded = 1; nFav = 0;
  f = fopen(FAVCFG, "r");
  if (!f) { static const char *DEF[4] = { "lector", "tasques", "@paquets", "@config" }; for (nFav = 0; nFav < 4; nFav++) snprintf(favKeys[nFav], 64, "%s", DEF[nFav]); return; }
  while (nFav < 4 && fgets(ln, sizeof(ln), f)) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) snprintf(favKeys[nFav++], 64, "%s", ln); }
  fclose(f);
}
static void fav_save(void) { FILE *f = fopen(FAVCFG, "w"); int i; if (!f) return; for (i = 0; i < nFav; i++) fprintf(f, "%s\n", favKeys[i]); fclose(f); }
static const char *app_key(int i) { return i < 0 ? NULL : i < nmods ? mods[i].id : i == nmods ? "@paquets" : i == nmods + 1 ? "@config" : NULL; }
static int app_by_key(const char *k) { int i; for (i = 0; i < nmods + 2; i++) if (!strcmp(app_key(i), k)) return i; return -1; }
static int fav_pos(const char *k) { int i; if (!k) return -1; for (i = 0; i < nFav; i++) if (!strcmp(favKeys[i], k)) return i; return -1; }
static void app_name(int i, char *d, int sz) {
  if (i < nmods) snprintf(d, (size_t)sz, "%s", mods[i].m->name);
  else if (i == nmods) snprintf(d, (size_t)sz, "Paquets");
  else if (i == nmods + 1) snprintf(d, (size_t)sz, "Configuració");
  else { size_t n; snprintf(d, (size_t)sz, "%s", extApps[i - nmods - 2]); n = strlen(d); if (n > 4) d[n - 4] = 0; }
}
/* Icona d'una aplicació dins una rajola de costat T (la rajola la dibuixa qui crida). pct = mida de la icona respecte de
   la rajola (el camí ocupa uns 3/4 de la seva caixa); fg i bg = colors del traç i de la rajola (per suavitzar les vores). */
static void app_icon(int i, int x, int y, int T, int pct, int fg, int bg) {
  int s = T * pct / 100, ix = x + (T - s) / 2, iy = y + (T - s) / 2;
  ICON_FG = fg; ICON_BG = bg; host.icon_fg = fg; host.icon_bg = bg;
  if (i < nmods) { if (mods[i].m->fn_icon) mods[i].m->fn_icon(ix, iy, s); }
  else if (i == nmods) icon_store(ix, iy, s);
  else if (i == nmods + 1) icon_system(ix, iy, s);
  else {
    char d[200], t[8];
    app_name(i, d, sizeof(d));
    t[0] = d[0]; t[1] = 0; if ((unsigned char)d[0] >= 0xC0) { t[1] = d[1]; t[2] = 0; }
    SetFont(T > 2 * U + U / 2 ? fHuge : fBig, fg); DrawTextRect(x, y, T, T, t, ALIGN_CENTER | VALIGN_MIDDLE);
  }
  ICON_FG = PAL(P_FG); ICON_BG = PAL(P_CARD); host.icon_fg = ICON_FG; host.icon_bg = ICON_BG;
}
static void v_desk(View *v) {
  char d[200]; int cols = 4, T = 3 * U + U / 4, cw = (SW - U) / cols, cellh = T + U + U / 2 + U / 4, dockH = 4 * U, dockY = SH - U / 2 - dockH, navh = U + U / 2;
  int y = SB + U / 4, i, k, total, nd = 0, dock[4], grid[240], ng = 0, avail, rows, per;
  ext_scan(); wl_collect(); home_load(); fav_load();
  wallOn = cfgBg == 2 && wall_draw();
  total = nmods + 2 + nExt;
  for (i = 0; i < nFav; i++) { int a = app_by_key(favKeys[i]); if (a >= 0) dock[nd++] = a; }
  for (i = 0; i < total && ng < 240; i++) { int in = 0; for (k = 0; k < nd; k++) if (dock[k] == i) in = 1; if (!in) grid[ng++] = i; }
  y += home_draw(y, (dockY - y) * 58 / 100) + U / 3;
  avail = dockY - U / 2 - y;
  rows = avail / cellh; if (rows < 1) rows = 1;
  if (ng > rows * cols) { rows = (avail - navh - U / 4) / cellh; if (rows < 1) rows = 1; }
  per = rows * cols;
  pages = ng ? (ng + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (k = 0, i = v->page * per; i < ng && k < per; i++, k++) {
    int a = grid[i], cx = U / 2 + (k % cols) * cw, cy = y + (k / cols) * cellh, tx = cx + (cw - T) / 2;
    ui_card(tx, cy, T, T, UI_LINE); app_icon(a, tx, cy, T, 62, PAL(P_FG), PAL(P_CARD)); app_name(a, d, sizeof(d));
    desk_text(cx + U / 4, cy + T + U / 4, cw - U / 2, U + U / 8, d, fSmall, PAL(P_FG), ALIGN_CENTER);
    add_hit(cx, cy, cw, cellh, A_APP, a);
  }
  if (pages > 1) {                            /* punts de pàgina, amb fletxes petites als costats */
    int py = dockY - U / 2 - navh, dw = U / 2, gap = U / 3, tot = pages * dw + (pages - 1) * gap + U / 2, dx = (SW - tot) / 2, b = navh;
    round_icon(U / 2, py, b, v->page > 0 ? 0 : 2, IC_BACK);
    round_icon(SW - U / 2 - b, py, b, v->page < pages - 1 ? 0 : 2, IC_FORWARD);
    for (k = 0; k < pages; k++) {
      int w = k == v->page ? dw + U / 2 : dw;
      rfill(dx, py + (navh - dw) / 2, w, dw, dw / 2, k == v->page ? PAL(P_FG) : PAL(P_LINE));
      dx += w + gap;
    }
    if (v->page > 0) add_hit(0, py - U / 4, SW / 2, navh + U / 2, A_DESK_PREV, 0);
    if (v->page < pages - 1) add_hit(SW / 2, py - U / 4, SW / 2, navh + U / 2, A_DESK_NEXT, 0);
  }
  if (nd) {                                   /* preferits: targeta flotant; el primer, ple */
    int slot = (SW - U - U / 2) / nd, t2 = 2 * U + U / 3, ty = dockY + U / 3;
    ui_card(U / 2, dockY, SW - U, dockH, UI_LINE);
    for (k = 0; k < nd; k++) {
      int sx = U / 2 + U / 4 + k * slot, tx = sx + (slot - t2) / 2, bg = k == 0 ? PAL(P_ACC) : PAL(P_TILE), fg = k == 0 ? PAL(P_ACCFG) : PAL(P_FG);
      rfill(tx, ty, t2, t2, t2 * 35 / 100, bg); app_icon(dock[k], tx, ty, t2, 76, fg, bg); app_name(dock[k], d, sizeof(d));
      SetFont(fSmall, PAL(P_FG)); DrawTextRect(sx + 2, ty + t2 + U / 8, slot - 4, dockH - t2 - U / 3 - U / 4, d, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
      add_hit(sx, dockY, slot, dockH, A_APP, dock[k]);
    }
  }
  if (!nmods && v->page == 0) {
    int old = pkgs_incompatible(); char t[260];
    if (old) snprintf(t, sizeof(t), "Hi ha %d paquets d'una versió anterior del sistema.\nObre «Paquets» i prem «Instal·la i actualitza-ho tot».", old);
    else snprintf(t, sizeof(t), "Encara no hi ha cap aplicació instal·lada.\nObre «Paquets» amb el Wi-Fi connectat per baixar-ne.");
    if (wallOn) ui_card(U / 2, y + cellh + U / 4, SW - U, 4 * U + U / 2, UI_LINE);
    SetFont(fText, PAL(P_FG));
    DrawTextRect(U, y + cellh + U / 2, SW - 2 * U, 4 * U, t, ALIGN_CENTER | VALIGN_TOP);
  }
}
/* Tria dels preferits */
static void v_favs(View *v) {
  int y = TOP, rh = 2 * U + U / 2, per, i, n = nmods + 2, tw = 2 * U, th = U + U / 2; char d[200], sub[80];
  fav_load();
  snprintf(sub, sizeof(sub), "%d de 4 · són a la barra de baix de l'inici", nFav);
  header("Preferits", sub);
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = (n + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    int ry = y + (i - v->page * per) * rh, on = fav_pos(app_key(i)) >= 0, t2 = rh - U / 4 - U / 2;
    ui_card(U / 2, ry, SW - U, rh - U / 4, on ? UI_LINE : UI_SOFT);
    rfill(U, ry + U / 4, t2, t2, t2 * 35 / 100, PAL(P_TILE)); app_icon(i, U, ry + U / 4, t2, 72, PAL(P_FG), PAL(P_TILE)); app_name(i, d, sizeof(d));
    SetFont(fBold, PAL(P_FG)); DrawTextRect(U + t2 + U / 2, ry, SW - 4 * U - t2 - tw, rh - U / 4, d, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    ui_toggle(SW - U - tw, ry + (rh - U / 4 - th) / 2, tw, th, on);
    add_hit(U / 2, ry, SW - U, rh - U / 4, A_FAV_TOGGLE, i);
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
      (void)ah;
      round_icon(ux, by, bs, up ? 0 : 2, IC_UP);
      round_icon(ux + bs + U / 4, by, bs, dn ? 0 : 2, IC_DOWN);
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


/* ───────────── avisos ───────────── */
#define MAXAV 40
typedef struct { long t; int read; char src[24], title[80], text[200]; } Aviso;
static Aviso av[MAXAV]; static int nAv;
static int avisos_unread(void) { int i, n = 0; for (i = 0; i < nAv; i++) if (!av[i].read) n++; return n; }
static void avisos_save(void) {
  FILE *f = fopen(CFG "escriptori_avisos.txt", "w"); int i;
  if (!f) return;
  for (i = 0; i < nAv; i++) fprintf(f, "%ld|%d|%s|%s|%s\n", av[i].t, av[i].read, av[i].src, av[i].title, av[i].text);
  fclose(f);
}
static void avisos_load(void) {
  FILE *f = fopen(CFG "escriptori_avisos.txt", "r"); char ln[400], *p[4], *q;
  nAv = 0;
  if (!f) return;
  while (nAv < MAXAV && fgets(ln, sizeof(ln), f)) {
    int k; ln[strcspn(ln, "\r\n")] = 0; q = ln;
    for (k = 0; k < 4; k++) { p[k] = q; q = q ? strchr(q, '|') : NULL; if (!q) break; *q++ = 0; }
    if (k < 4 || !q) continue;
    av[nAv].t = atol(p[0]); av[nAv].read = atoi(p[1]);
    snprintf(av[nAv].src, sizeof(av[nAv].src), "%s", p[2]); snprintf(av[nAv].title, sizeof(av[nAv].title), "%s", p[3]); snprintf(av[nAv].text, sizeof(av[nAv].text), "%s", q);
    nAv++;
  }
  fclose(f);
}
/* Afegeix un avís (el més nou primer). Si ja n'hi ha un de no llegit amb el mateix origen i títol, el substitueix. No refresca la pantalla. */
static void avisos_add(const char *src, const char *title, const char *text) {
  Aviso a; int i;
  memset(&a, 0, sizeof(a)); a.t = (long)time(NULL);
  snprintf(a.src, sizeof(a.src), "%s", src ? src : ""); snprintf(a.title, sizeof(a.title), "%s", title ? title : ""); snprintf(a.text, sizeof(a.text), "%s", text ? text : "");
  for (i = 0; a.src[i]; i++) if (a.src[i] == '|' || a.src[i] == '\n') a.src[i] = ' ';
  for (i = 0; a.title[i]; i++) if (a.title[i] == '|' || a.title[i] == '\n') a.title[i] = ' ';
  for (i = 0; a.text[i]; i++) if (a.text[i] == '|' || a.text[i] == '\n') a.text[i] = ' ';
  for (i = 0; i < nAv; i++) if (!av[i].read && !strcmp(av[i].src, a.src) && !strcmp(av[i].title, a.title)) { memmove(&av[1], &av[0], sizeof(Aviso) * (size_t)i); av[0] = a; avisos_save(); return; }
  if (nAv >= MAXAV) nAv = MAXAV - 1;
  memmove(&av[1], &av[0], sizeof(Aviso) * (size_t)nAv); av[0] = a; nAv++;
  avisos_save();
}
/* Avisos del sistema; s'avalua cada minut. */
static void avisos_poll(void) {
  static int lowArmed = 1; int p = GetBatteryPower();
  if (p >= 0 && p <= 15 && !IsCharging() && lowArmed) { char t[80]; snprintf(t, sizeof(t), "Queda un %d%% de bateria.", p); avisos_add("Sistema", "Bateria baixa", t); lowArmed = 0; }
  if (p > 25 || IsCharging()) lowArmed = 1;
}
static void pkg_notify(void) {
  int i, pend = 0; char t[80];
  for (i = 0; i < ncat; i++) { int pv = pkg_ver(cat[i].id); if (pv && (pv < cat[i].ver || mod_find(cat[i].id) < 0)) pend++; }
  if (!pend) return;
  snprintf(t, sizeof(t), pend == 1 ? "Hi ha %d paquet per actualitzar." : "Hi ha %d paquets per actualitzar.", pend);
  avisos_add("Sistema", "Paquets per actualitzar", t);
}
static void v_avisos(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 4 * U, per, i, bw = (SW - U - U / 3) / 2; char sub[100];
  snprintf(sub, sizeof(sub), "%d sense llegir · toca per llegir; torna a tocar per esborrar", avisos_unread());
  header("Avisos", sub);
  if (nAv) {
    button(U / 2, y, bw, bh, "Marca-ho tot llegit", 0, A_AV_READ, 0);
    button(U / 2 + bw + U / 3, y, bw, bh, "Esborra-ho tot", 0, A_AV_CLEAR, 0);
    y += bh + U / 3;
  } else { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 3 * U, "No tens cap avís.", ALIGN_LEFT | VALIGN_TOP); }
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = nAv ? (nAv + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < nAv && i < (v->page + 1) * per; i++) {
    int ry = y + (i - v->page * per) * rh, nw = 6 * U, un = !av[i].read; char tm[32], head[120]; time_t tt = (time_t)av[i].t;
    ui_card(U / 2, ry, SW - U, rh - U / 4, un ? UI_LINE : UI_SOFT);
    if (un) rfill(U, ry + U / 2 + U / 8, U / 2, U / 2, U / 4, PAL(P_FG));
    { time_t nw = time(NULL); struct tm a = *localtime(&tt), b = *localtime(&nw); long dd = (long)((nw - b.tm_hour * 3600 - b.tm_min * 60 - b.tm_sec) - tt);
      if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) snprintf(tm, sizeof(tm), "Avui %d:%02d", a.tm_hour, a.tm_min);
      else if (dd > 0 && dd <= 86400) snprintf(tm, sizeof(tm), "Ahir %d:%02d", a.tm_hour, a.tm_min);
      else snprintf(tm, sizeof(tm), "%d/%d %d:%02d", a.tm_mday, a.tm_mon + 1, a.tm_hour, a.tm_min); }
    snprintf(head, sizeof(head), "%s", av[i].title);
    SetFont(fBold, un ? BLACK : DGRAY); DrawTextRect(U + U, ry + U / 4, SW - 2 * U - U - nw - U / 2, U * 3 / 2, head, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, DGRAY); DrawTextRect(SW - U - nw, ry + U / 4, nw, U * 3 / 2, tm, ALIGN_RIGHT | VALIGN_MIDDLE);
    snprintf(head, sizeof(head), "%s · %s", av[i].src, av[i].text);
    SetFont(fSmall, DGRAY); DrawTextRect(U + U, ry + U * 7 / 4, SW - 3 * U, rh - U * 2 - U / 4, head, ALIGN_LEFT | VALIGN_TOP | DOTS);
    add_hit(U / 2, ry, SW - U, rh - U / 4, A_AV_ONE, i);
  }
}

/* ───────────── tauler ràpid ───────────── */
static int light_get(void) { int v = GetFrontlightState(); return v == INT_MIN ? -1 : v; }
static void v_quick(View *v) {
  int gap = U / 2, tw = (SW - U - gap) / 2, th = 5 * U + U / 2, x0 = U / 2, x1 = U / 2 + tw + gap, y = TOP, net = QueryNetwork(), lv = light_get(), bs = 2 * U + U / 4, is = U * 3 / 2, bh = 2 * U + U / 2, nu = avisos_unread(), i;
  char t[100], st[60]; int solid, lh = 5 * U + U / 4;
  (void)v;
  snprintf(t, sizeof(t), "Bateria %d%%%s", GetBatteryPower(), IsCharging() ? " · carregant" : "");
  header("Tauler ràpid", t);
  /* fila 1: Wi-Fi i avisos */
  solid = (net & NET_WIFIREADY) != 0;
  snprintf(st, sizeof(st), "%s", solid ? "Connectat · toca per desconnectar" : (net & NET_WIFI) ? "Encès · toca per connectar" : "Apagat · toca per connectar");
  ui_tile(x0, y, tw, th, "Wi-Fi", st, solid, A_QWIFI, 0);
  ic_signal(x0 + U + U / 2, y + U + U / 2, U * 3 / 2, wifi_level(), solid ? PAL(P_ACCFG) : PAL(P_FG), solid ? PAL(P_SUB) : PAL(P_SHADOW), solid ? -1 : PAL(P_FG));
  if (nu) snprintf(st, sizeof(st), nu == 1 ? "%d de nou" : "%d de nous", nu); else snprintf(st, sizeof(st), "Res de nou");
  ui_tile(x1, y, tw, th, "Avisos", st, 0, A_AVISOS, 0);
  ic_path(IC_BELL, x1 + U * 3 / 4, y + U * 3 / 4, is, 1.8, PAL(P_FG), PAL(P_CARD));
  /* llum: barra de 10 nivells entre els botons de menys i més */
  y += th + gap;
  ui_card(x0, y, SW - U, lh, UI_LINE);
  ic_path(IC_SUN, x0 + U * 3 / 4, y + U / 2, is, 1.8, PAL(P_FG), PAL(P_CARD));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(x0 + 3 * U, y + U / 4, 8 * U, 2 * U, "Llum", ALIGN_LEFT | VALIGN_MIDDLE);
  if (lv >= 0) {
    int by = y + 2 * U + U / 2, sx = x0 + U / 2 + bs + U / 2, sw = SW - U - 2 * (U / 2 + bs + U / 2), g = U / 5, w1 = (sw - 9 * g) / 10, sh = U + U / 4, on = (lv + 5) / 10;
    snprintf(st, sizeof(st), "%d%%", lv);
    SetFont(fBold, PAL(P_FG)); DrawTextRect(SW - U - 6 * U, y + U / 4, 5 * U, 2 * U, st, ALIGN_RIGHT | VALIGN_MIDDLE);
    round_icon(x0 + U / 2, by, bs, 0, IC_MINUS);
    round_icon(SW - U - bs, by, bs, 0, IC_PLUS);
    for (i = 0; i < 10; i++) rfill(sx + i * (w1 + g), by + (bs - sh) / 2, w1, sh, U / 4, i < on ? PAL(P_ACC) : PAL(P_TILE));
    add_hit(x0, by - U / 4, U / 2 + bs + U / 2, bs + U / 2, A_QLIGHT_DN, 0);
    add_hit(SW - U - bs - U / 2, by - U / 4, bs + U, bs + U / 2, A_QLIGHT_UP, 0);
  } else { SetFont(fText, PAL(P_SUB)); DrawTextRect(x0 + U, y + 2 * U + U / 2, SW - 3 * U, 2 * U, "Aquest lector no informa de la llum frontal.", ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  /* fila 3: neteja de pantalla i aparença; fila 4: configuració */
  y += lh + gap;
  ui_tile(x0, y, tw, th - U, "Neteja la pantalla", "Refresc complet", 0, A_QCLEAN, 0);
  ic_path(IC_REFRESH, x0 + U * 3 / 4, y + U / 2, is, 1.8, PAL(P_FG), PAL(P_CARD));
  ui_tile(x1, y, tw, th - U, "Aparença", cfgTheme ? "Tema fosc" : "Tema clar", 0, A_CFG_GO, V_LOOK);
  ic_path(IC_CONTRAST, x1 + U * 3 / 4, y + U / 2, is, 1.8, PAL(P_FG), PAL(P_CARD));
  y += th - U + gap;
  ui_tile(x0, y, SW - U, th - U, "Configuració", "Tots els ajustos", 0, A_QGO, 0);
  ic_path(IC_SLIDERS, x0 + U * 3 / 4, y + U / 2, is, 1.8, PAL(P_FG), PAL(P_CARD));
  button(U / 2, SH - U / 2 - bh, SW - U, bh, "Surt de l'Escriptori", 0, A_EXIT, 0);
}
static void v_store(View *v) {
  static const char *FL[3] = { "Tots", "Instal·lats", "Per actualitzar" };
  int y = TOP, bh = 2 * U + U / 4, rh = 3 * U, per, i, k, bw = (SW - U - U / 3) / 2, inst = 0, pend = 0, upd = 0, idx[MAXCAT], n = 0, sh = 2 * U, sw = (SW - U - 12) / 3; char sub[100];
  for (i = 0; i < ncat; i++) {
    int pv = pkg_ver(cat[i].id), u = pv && (pv < cat[i].ver || mod_find(cat[i].id) < 0);
    if (pv) inst++;
    if (u) upd++;
    if (!pv || u) pend++;
    if (v->a == 0 || (v->a == 1 && pv) || (v->a == 2 && u)) idx[n++] = i;
  }
  snprintf(sub, sizeof(sub), "%d al catàleg · %d instal·lats", ncat, inst);
  header("Paquets", sub);
  { char l3[40]; const char *labs[3]; snprintf(l3, sizeof(l3), upd ? "%s · %d" : "%s", FL[2], upd); labs[0] = FL[0]; labs[1] = FL[1]; labs[2] = l3;
    seg(U / 2, y, SW - U, sh, labs, 3, v->a, A_STORE_FILTER); (void)sw; (void)k; }
  y += sh + U / 3;
  button(U / 2, y, bw, bh, "Actualitza el catàleg", 0, A_STORE_REFRESH, 0);
  if (pend) button(U / 2 + bw + U / 3, y, bw, bh, inst ? "Instal·la i actualitza-ho tot" : "Instal·la-ho tot", 1, A_STORE_ALL, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = n ? (n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!ncat) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 5 * U, "Encara no s'ha baixat el catàleg.\nConnecta el Wi-Fi i prem «Actualitza el catàleg».", ALIGN_LEFT | VALIGN_TOP); }
  else if (!n) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 3 * U, v->a == 2 ? "No hi ha res per actualitzar." : "Encara no hi ha cap paquet instal·lat.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    const Pkg *q = &cat[idx[i]]; int pv = pkg_ver(q->id), m = mod_find(q->id), u = pv && (pv < q->ver || m < 0), ch = rh - U / 4, T = ch - U / 2, pw = 5 * U + U / 2, ph = 2 * U, px = SW - U - pw, py = y + (ch - ph) / 2;
    const char *lab = !pv ? "Instal·la" : u ? "Actualitza" : "Obre";
    ui_card(U / 2, y, SW - U, ch, UI_LINE);
    ui_card(U / 2 + U / 4, y + U / 4, T, T, UI_SOFT);
    if (m >= 0) app_icon(m, U / 2 + U / 4, y + U / 4, T, 72, PAL(P_FG), PAL(P_TILE)); else app_icon(nmods, U / 2 + U / 4, y + U / 4, T, 72, PAL(P_FG), PAL(P_TILE));
    SetFont(fBold, BLACK); DrawTextRect(U + T + U / 4, y + U / 6, px - 2 * U - T, ch / 2 - U / 8, q->name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, DGRAY); DrawTextRect(U + T + U / 4, y + ch / 2 - U / 8, px - 2 * U - T, ch / 2 - U / 8, q->desc, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, ch, A_PKG, idx[i]);
    if (!pv || u) { pill(px, py, pw, ph, 1); SetFont(fBold, PAL(P_ACCFG)); } else { rfill(px, py, pw, ph, ph / 2, PAL(P_TILE)); SetFont(fBold, PAL(P_FG)); }
    DrawTextRect(px, py, pw, ph, lab, ALIGN_CENTER | VALIGN_MIDDLE);
    if (!pv || u) add_hit(px - U / 4, y, pw + U / 2, ch, A_PKG_INSTALL, idx[i]); else if (m >= 0) add_hit(px - U / 4, y, pw + U / 2, ch, A_APP, m);
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
/* ───────────── configuració ───────────── */
static int cfg_group(int y, const char *label, int nrows, int rh) {      /* etiqueta + targeta del grup; retorna la y de la primera fila */
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, U + U / 4, label, ALIGN_LEFT | VALIGN_MIDDLE);
  ui_card(U / 2, y + U + U / 4, SW - U, nrows * rh, UI_LINE);
  return y + U + U / 4;
}
enum { CR_CHEV, CR_ON, CR_OFF, CR_NONE };
static void cfg_row(int y, int rh, int first, const char *title, const char *value, int kind, int act, int arg) {
  int xr = SW - U, tw = 2 * U, th = U + U / 4, cs = U;
  if (!first) FillArea(U, y, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(3 * U + U / 4, y, SW / 2 - 3 * U, rh, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  if (kind == CR_CHEV) { ic_path(IC_FORWARD, xr - cs, y + (rh - cs) / 2, cs, 2.2, PAL(P_SUB), PAL(P_CARD)); xr -= cs + U / 4; }
  else if (kind == CR_ON || kind == CR_OFF) { ui_toggle(xr - tw, y + (rh - th) / 2, tw, th, kind == CR_ON); xr -= tw + U / 2; }
  if (value && value[0]) { SetFont(fSmall, PAL(P_SUB)); DrawTextRect(SW / 2 - U, y, xr - (SW / 2 - U), rh, value, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS); }
  if (act) add_hit(U / 2, y, SW - U, rh, act, arg);
}
static void row_icon(const char *ic, int y, int rh) { int s = U + U / 3; ic_path(ic, U + U / 4, y + (rh - s) / 2, s, 1.8, PAL(P_FG), PAL(P_CARD)); }
static void v_config(View *v) {
  int y = TOP - U / 4, rh = 2 * U + U / 8, g = U / 4, net = QueryNetwork(), lv = light_get(), nu = avisos_unread(), bs, yy; char t[80];
  (void)v; cfg_load(); wl_collect(); home_load(); fav_load();
  header("Configuració", "");
  yy = cfg_group(y, "CONNEXIÓ", 1, rh);
  cfg_row(yy, rh, 1, "Wi-Fi", (net & NET_WIFIREADY) ? "Connectat" : (net & NET_WIFI) ? "Encès, sense connexió" : "Apagat", (net & NET_WIFIREADY) ? CR_ON : CR_OFF, A_QWIFI, 0);
  ic_signal(U + U / 4 + (U + U / 3) / 2, yy + rh / 2, U + U / 8, wifi_level(), PAL(P_FG), PAL(P_SHADOW), -1);
  y = yy + rh + g;
  yy = cfg_group(y, "PANTALLA", 3, rh);
  snprintf(t, sizeof(t), "%s · %s", cfgTheme ? "Fosc" : "Clar", cfgFont[0] ? cfgFont : "font del lector");
  cfg_row(yy, rh, 1, "Aparença", t, CR_CHEV, A_CFG_GO, V_LOOK);
  row_icon(IC_CONTRAST, yy, rh);
  if (lv >= 0) snprintf(t, sizeof(t), "%d%%", lv); else snprintf(t, sizeof(t), "No disponible");
  bs = rh - U / 2;
  cfg_row(yy + rh, rh, 0, "Llum", "", CR_NONE, 0, 0);
  row_icon(IC_SUN, yy + rh, rh);
  if (lv >= 0) {
    round_icon(SW - U - bs, yy + rh + U / 4, bs, 0, IC_PLUS);
    round_icon(SW - U - 2 * bs - 3 * U, yy + rh + U / 4, bs, 0, IC_MINUS);
    SetFont(fBold, PAL(P_FG)); DrawTextRect(SW - U - bs - 3 * U, yy + rh, 3 * U, rh, t, ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(SW - U - bs - U, yy + rh, bs + U + U / 2, rh, A_QLIGHT_UP, 0);
    add_hit(SW - U - 2 * bs - 3 * U - U / 2, yy + rh, bs + U + U / 2, rh, A_QLIGHT_DN, 0);
  } else { SetFont(fSmall, PAL(P_SUB)); DrawTextRect(SW / 2, yy + rh, SW / 2 - U, rh, t, ALIGN_RIGHT | VALIGN_MIDDLE); }
  if (cfgFull <= 1) snprintf(t, sizeof(t), "A cada pantalla"); else snprintf(t, sizeof(t), "Cada %d pantalles", cfgFull);
  cfg_row(yy + 2 * rh, rh, 0, "Refresc complet", t, CR_CHEV, A_CFG_FULL, 0);
  row_icon(IC_REFRESH, yy + 2 * rh, rh);
  y = yy + 3 * rh + g;
  yy = cfg_group(y, "ESCRIPTORI", 4, rh);
  { int i, n = 0; for (i = 0; i < nHome; i++) if (wl_find(homeKeys[i]) >= 0) n++; snprintf(t, sizeof(t), n == 1 ? "%d widget" : "%d widgets", n); }
  cfg_row(yy, rh, 1, "Inici i widgets", t, CR_CHEV, A_CFG_GO, V_HOME);
  row_icon(IC_GRID, yy, rh);
  snprintf(t, sizeof(t), "%d de 4", nFav);
  cfg_row(yy + rh, rh, 0, "Preferits", t, CR_CHEV, A_CFG_GO, V_FAVS);
  row_icon(IC_STAR, yy + rh, rh);
  if (nu) snprintf(t, sizeof(t), nu == 1 ? "%d de nou" : "%d de nous", nu); else snprintf(t, sizeof(t), "Res de nou");
  cfg_row(yy + 2 * rh, rh, 0, "Avisos", t, CR_CHEV, A_AVISOS, 0);
  row_icon(IC_BELL, yy + 2 * rh, rh);
  snprintf(t, sizeof(t), nmods == 1 ? "%d instal·lat" : "%d instal·lats", nmods);
  cfg_row(yy + 3 * rh, rh, 0, "Paquets", t, CR_CHEV, A_CFG_GO, V_STORE);
  row_icon(IC_PACKAGE, yy + 3 * rh, rh);
  y = yy + 4 * rh + g;
  yy = cfg_group(y, "SISTEMA", 3, rh);
  if (updState == 2) snprintf(t, sizeof(t), "Versió %s disponible", updName); else if (updState == 1) snprintf(t, sizeof(t), "Al dia"); else if (updState == 3) snprintf(t, sizeof(t), "Instal·lada: reobre l'Escriptori"); else snprintf(t, sizeof(t), "Versió %s", APP_VERNAME);
  cfg_row(yy, rh, 1, "Actualització", t, CR_CHEV, A_CFG_GO, V_UPDATE);
  row_icon(IC_DOWNLOAD, yy, rh);
  { const char *m = GetDeviceModel(); snprintf(t, sizeof(t), "%s", m && m[0] ? m : ""); }
  cfg_row(yy + rh, rh, 0, "Sobre el dispositiu", t, CR_CHEV, A_CFG_GO, V_ABOUT);
  row_icon(IC_INFO, yy + rh, rh);
  cfg_row(yy + 2 * rh, rh, 0, "Surt de l'Escriptori", "", CR_NONE, A_EXIT, 0);
  row_icon(IC_POWER, yy + 2 * rh, rh);
}
/* Control segmentat: n opcions, la triada plena. Cada opció crida act amb el seu índex. */
static void seg(int x, int y, int w, int h, const char **lab, int n, int cur, int act) {
  int i, sw = (w - 8) / n;
  rfill(x, y, w, h, h / 2, PAL(P_TILE));
  for (i = 0; i < n; i++) {
    int sx = x + 4 + i * sw;
    if (i == cur) rfill(sx, y + 4, sw, h - 8, (h - 8) / 2, PAL(P_ACC));
    SetFont(fBold, i == cur ? PAL(P_ACCFG) : PAL(P_FG)); DrawTextRect(sx + 4, y, sw - 8, h, lab[i], ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    add_hit(sx, y, sw, h, act, i);
  }
}
/* ───────────── Aparença ───────────── */
static void v_look(View *v) {
  static const char *TH[2] = { "Clar", "Fosc" }, *BGL[3] = { "Cap", "Gris", "Imatge" }, *CO[2] = { "Suau", "Fort" };
  static const char *HINT[3] = { "Sense fons: blanc net, el que deixa menys rastre a la pantalla.",
                                 "Gris suau amb targetes blanques.",
                                 "La primera imatge (JPG o PNG) de la carpeta «Fons» de la memòria, posada per FTP. Les targetes queden a sobre." };
  int y = TOP, ph = 4 * U + U / 2, rh = 2 * U + U / 2, sh = 2 * U, x2 = SW / 2 - U / 2, w2 = SW - U - U / 2 - x2, yy, i, d = rh - U / 2;
  time_t now = time(NULL); struct tm *tm = localtime(&now); char t[160];
  (void)v; cfg_load();
  header("Aparença", "");
  ui_card(U / 2, y, SW - U, ph, UI_LINE);                         /* vista prèvia */
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  SetFont(fHuge, PAL(P_FG)); DrawTextRect(U + U / 4, y, 9 * U, ph, t, ALIGN_LEFT | VALIGN_MIDDLE);
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U * 11, y + U, SW - 12 * U, U + U / 2, "Vista prèvia", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fText, PAL(P_SUB)); DrawTextRect(U * 11, y + U * 5 / 2, SW - 12 * U, U + U / 2, "Així es veu el text", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  y += ph + U / 2;
  ui_card(U / 2, y, SW - U, 5 * rh, UI_LINE);
  yy = y;
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy, x2 - U, rh, "Tema", ALIGN_LEFT | VALIGN_MIDDLE);
  seg(x2, yy + (rh - sh) / 2, w2, sh, TH, 2, cfgTheme, A_LOOK_THEME);
  yy += rh; FillArea(U, yy, SW - 2 * U, 2, PAL(P_LINE));
  cfg_row(yy, rh, 1, "", cfgFont[0] ? cfgFont : "La del lector", CR_CHEV, A_CFG_GO, V_FONTS);
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy, x2 - U, rh, "Font", ALIGN_LEFT | VALIGN_MIDDLE);
  yy += rh; FillArea(U, yy, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy, x2 - U, rh, "Mida del text", ALIGN_LEFT | VALIGN_MIDDLE);
  for (i = 0; i < 4; i++) {
    int bx = SW - U - (4 - i) * (d + U / 4) + U / 4, by = yy + (rh - d) / 2, on = i == cfgSize; ifont *f = OpenFont(DEFAULTFONTB, U * (6 + 2 * i) / 10 + U / 4, 1);
    rfill(bx, by, d, d, d * 35 / 100, on ? PAL(P_ACC) : PAL(P_TILE));
    if (f) { SetFont(f, on ? PAL(P_ACCFG) : PAL(P_FG)); DrawTextRect(bx, by, d, d, "A", ALIGN_CENTER | VALIGN_MIDDLE); CloseFont(f); }
    add_hit(bx - U / 8, yy, d + U / 4, rh, A_LOOK_SIZE, i);
  }
  yy += rh; FillArea(U, yy, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy, x2 - U, rh, "Fons de l'inici", ALIGN_LEFT | VALIGN_MIDDLE);
  seg(x2, yy + (rh - sh) / 2, w2, sh, BGL, 3, cfgBg, A_LOOK_BG);
  yy += rh; FillArea(U, yy, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy, x2 - U, rh, "Contrast", ALIGN_LEFT | VALIGN_MIDDLE);
  seg(x2, yy + (rh - sh) / 2, w2, sh, CO, 2, cfgCon, A_LOOK_CON);
  y += 5 * rh + U / 2;
  snprintf(t, sizeof(t), "%s", HINT[cfgBg]);
  if (cfgBg == 2) { char p[600]; wall_find(p, sizeof(p)); if (!p[0]) snprintf(t, sizeof(t), "La carpeta «Fons» és buida: posa-hi una imatge JPG o PNG per FTP."); }
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, 3 * U, t, ALIGN_LEFT | VALIGN_TOP);
  y += 3 * U;
  if (cfgTheme) { SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, 3 * U, "Amb el tema fosc la pantalla deixa més rastre: si cal, fes servir «Neteja la pantalla» o un refresc complet més sovint.", ALIGN_LEFT | VALIGN_TOP); }
}
/* ───────────── Tria de font ───────────── */
static void v_fonts(View *v) {
  int y = TOP, rh = 3 * U, sampleH = 6 * U, per, i, n, chip = rh - U / 2, top; char sub[100], r[140], b[140]; char **L;
  fam_scan(); L = EnumFonts();
  n = nfam + 1;
  snprintf(sub, sizeof(sub), nfam == 1 ? "%d font · en pots afegir a la carpeta «Fonts»" : "%d fonts · en pots afegir a la carpeta «Fonts»", nfam);
  header("Font", sub);
  per = (BOTTOM - sampleH - U / 2 - y) / rh; if (per < 1) per = 1;
  pages = (n + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  top = y;
  { int cnt = n - v->page * per; if (cnt > per) cnt = per; ui_card(U / 2, y, SW - U, cnt * rh, UI_LINE); }
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    const char *fam = i == 0 ? "" : famN[i - 1]; int on = !strcmp(fam, cfgFont); ifont *fr, *fb;
    if (i > v->page * per) FillArea(U, y, SW - 2 * U, 2, PAL(P_LINE));
    font_names(L, fam, r, b, sizeof(r));
    fb = OpenFont(b, U * 11 / 10, 1); fr = OpenFont(r, U, 1);
    rfill(U, y + U / 4, chip, chip, chip * 35 / 100, on ? PAL(P_ACC) : PAL(P_TILE));
    SetFont(fb ? fb : fBig, on ? PAL(P_ACCFG) : PAL(P_FG)); DrawTextRect(U, y + U / 4, chip, chip, "Aa", ALIGN_CENTER | VALIGN_MIDDLE);
    SetFont(fr ? fr : fBold, PAL(P_FG)); DrawTextRect(U + chip + U / 2, y + U / 4, SW - 5 * U - chip, rh / 2, i == 0 ? "La del lector" : fam, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U + chip + U / 2, y + rh / 2, SW - 5 * U - chip, rh / 2 - U / 4, i == 0 ? "Per defecte" : famUser[i - 1] ? "Afegida per tu" : "Del lector", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (on) ic_path(IC_CHECK, SW - U - U * 3 / 2, y + (rh - U * 3 / 2) / 2, U * 3 / 2, 2.6, PAL(P_FG), PAL(P_CARD));
    if (fr) CloseFont(fr);
    if (fb) CloseFont(fb);
    add_hit(U / 2, y, SW - U, rh, A_FONT_PICK, i);
    y += rh;
  }
  (void)top;
  y = BOTTOM - sampleH;                                     /* mostra amb la font triada */
  rfill(U / 2, y, SW - U, sampleH, U, PAL(P_ACC));
  { ifont *f1, *f2; font_names(L, cfgFont, r, b, sizeof(r)); f1 = OpenFont(b, U * 3 / 2, 1); f2 = OpenFont(r, U * 9 / 10, 1);
    SetFont(fSmall, PAL(P_ACCFG)); DrawTextRect(U + U / 4, y + U / 3, SW - 3 * U, U, "MOSTRA", ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(f1 ? f1 : fBig, PAL(P_ACCFG)); DrawTextRect(U + U / 4, y + U + U / 3, SW - 3 * U, U * 2, "Bon dia, 12:41", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(f2 ? f2 : fText, PAL(P_ACCFG)); DrawTextRect(U + U / 4, y + 3 * U + U / 3, SW - 3 * U, 2 * U + U / 2, "Jove xef, porti whisky amb quinze glaçons d'hidrogen, coi! 0123456789", ALIGN_LEFT | VALIGN_TOP);
    if (f1) CloseFont(f1);
    if (f2) CloseFont(f2); }
}
static void v_update(View *v) {
  char t[500]; int y = TOP, bh = 2 * U + U / 2, net = QueryNetwork();
  (void)v;
  snprintf(t, sizeof(t), "Tens l'Escriptori %s (compilació %d)", APP_VERNAME, APP_BUILD);
  header("Actualització", t);
  if (updState == 0) snprintf(t, sizeof(t), "Comprova si hi ha una versió nova de l'Escriptori.");
  else if (updState == 1) snprintf(t, sizeof(t), "Ja tens l'última versió (el servidor té la compilació %d).", updBuild);
  else if (updState == 2) snprintf(t, sizeof(t), "Hi ha la versió %s disponible (compilació %d).%s%s", updName, updBuild, updNotes[0] ? "\n" : "", updNotes);
  else if (updState == 3) snprintf(t, sizeof(t), "S'ha instal·lat la versió %s.\nSurt de l'Escriptori i torna'l a obrir.", updName);
  else snprintf(t, sizeof(t), "No s'ha pogut completar. Torna-ho a provar amb el Wi-Fi connectat.");
  ui_card(U / 2, y, SW - U, 7 * U, UI_LINE);
  SetFont(fText, BLACK); DrawTextRect(U + U / 4, y + U / 2, SW - 2 * U - U / 2, 6 * U, t, ALIGN_LEFT | VALIGN_TOP); y += 7 * U + U / 2;
  if (updState == 2) button(U / 2, y, SW - U, bh, "Descarrega i instal·la", 1, A_UPD_INSTALL, 0);
  else if (updState == 3) button(U / 2, y, SW - U, bh, "Surt de l'Escriptori", 1, A_EXIT, 0);
  else button(U / 2, y, SW - U, bh, "Comprova si hi ha una versió nova", 1, A_UPD_CHECK, 0);
  y += bh + U / 3;
  if (!(net & NET_WIFIREADY)) button(U / 2, y, SW - U, bh, "Connecta el Wi-Fi", 0, A_WIFI, 0);
}
static void kv(int y, int rh, int first, const char *k, const char *val) {
  if (!first) FillArea(U, y, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, y, SW * 4 / 10, rh, k, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fText, PAL(P_SUB)); DrawTextRect(SW * 4 / 10, y, SW * 6 / 10 - U - U / 4, rh, val && val[0] ? val : "—", ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
}
static void fmt_gb(char *o, int sz, double bytes) { long t = (long)(bytes / 107374182.4 + 0.5); snprintf(o, (size_t)sz, "%ld,%ld GB", t / 10, t % 10); }
static void v_about(View *v) {
  int y = TOP, rh = U * 7 / 4, g = U / 4, hh = 4 * U + U / 2, net = QueryNetwork(), yy, dw, dh, bh = 2 * U + U / 4; char t[200], a[40], b[40]; struct statvfs sv;
  const char *model = GetDeviceModel(), *serial = GetSerialNumber(), *fw = GetSoftwareVersion();
  (void)v;
  header("Sobre el dispositiu", "");
  rfill(U / 2, y, SW - U, hh, U, PAL(P_ACC));                              /* targeta del lector */
  dh = hh - U; dw = dh * 7 / 10;
  rcard(U + U / 4, y + U / 2, dw, dh, U / 3, 3, PAL(P_ACCFG), PAL(P_ACC)); rcard(U + U / 4 + dw / 8, y + U / 2 + dw / 8, dw - dw / 4, dh - dw / 4 - dh / 6, 3, 3, PAL(P_ACCFG), PAL(P_ACC));
  FillArea(U + U / 4 + dw * 3 / 8, y + U / 2 + dh - dh / 10, dw / 4, 3, PAL(P_ACCFG));
  SetFont(fHead ? fHead : fTitle, PAL(P_ACCFG)); DrawTextRect(2 * U + dw, y + U / 2, SW - 4 * U - dw, hh / 2 - U / 4, model && model[0] ? model : "Lector PocketBook", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  snprintf(t, sizeof(t), "Escriptori %s · compilació %d", APP_VERNAME, APP_BUILD);
  SetFont(fText, PAL(P_ACCFG)); DrawTextRect(2 * U + dw, y + hh / 2 + U / 8, SW - 4 * U - dw, U + U / 2, t, ALIGN_LEFT | VALIGN_TOP | DOTS);
  y += hh + g;
  yy = cfg_group(y, "DISPOSITIU", 4, rh);
  kv(yy, rh, 1, "Model", model);
  kv(yy + rh, rh, 0, "Número de sèrie", serial);
  kv(yy + 2 * rh, rh, 0, "Programari del lector", fw);
  snprintf(t, sizeof(t), "%d × %d · tinta electrònica", ScreenWidth(), ScreenHeight());
  kv(yy + 3 * rh, rh, 0, "Pantalla", t);
  y = yy + 4 * rh + g;
  yy = cfg_group(y, "ESTAT", 3, rh);
  ui_card(U / 2, yy, SW - U, 2 * rh + rh + U / 2, UI_LINE);   /* la primera fila és més alta (barra d'espai) */
  if (statvfs(FLASHDIR, &sv) == 0 && sv.f_blocks > 0) {
    double tot = (double)sv.f_blocks * (double)sv.f_frsize, fr = (double)sv.f_bavail * (double)sv.f_frsize; int bw = SW - 2 * U - U / 2, used = (int)((tot - fr) / tot * bw);
    fmt_gb(a, sizeof(a), fr); fmt_gb(b, sizeof(b), tot); snprintf(t, sizeof(t), "%s lliures de %s", a, b);
    kv(yy, rh, 1, "Emmagatzematge", t);
    rfill(U + U / 4, yy + rh - U / 8, bw, U / 2, U / 4, PAL(P_TILE)); if (used > 6) rfill(U + U / 4, yy + rh - U / 8, used, U / 2, U / 4, PAL(P_FG));
  } else kv(yy, rh, 1, "Emmagatzematge", "");
  yy += rh + U / 2;
  snprintf(t, sizeof(t), "%d%% · %s", batt_pct(), IsCharging() ? "carregant" : "sense carregar");
  kv(yy, rh, 0, "Bateria", t);
  if (net & NET_WIFIREADY) snprintf(t, sizeof(t), "Connectat · senyal %d de 4", wifi_level()); else snprintf(t, sizeof(t), "%s", (net & NET_WIFI) ? "Encès, sense connexió" : "Apagat");
  kv(yy + rh, rh, 0, "Wi-Fi", t);
  y = yy + 2 * rh + g;
  yy = cfg_group(y, "ESCRIPTORI", 3, rh);
  snprintf(t, sizeof(t), "%s · compilació %d · API %d", APP_VERNAME, APP_BUILD, ESC_API);
  kv(yy, rh, 1, "Versió", t);
  snprintf(t, sizeof(t), "%d instal·lats · %d al catàleg", nmods, ncat);
  kv(yy + rh, rh, 0, "Paquets", t);
  kv(yy + 2 * rh, rh, 0, "Codi", "github.com/sarnaloka/pb_launcher");
  button(U / 2, SH - U / 2 - bh, SW - U, bh, "Comprova si hi ha una versió nova", 1, A_ABOUT_UPD, 0);
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
    ClearScreen(); FillArea(0, 0, SW, SH, PAL(P_BG)); nhits = 0; pages = 1; footY = SH - NAVH - U / 4; wallOn = 0;
    statusbar(1);
    if (v->mod >= 0) mods[v->mod].m->fn_draw(v);
    else switch (v->type) {
      case V_DESK: v_desk(v); break;
      case V_STORE: v_store(v); break;
      case V_PKG: v_pkg(v); break;
      case V_SYSTEM: v_config(v); break;
      case V_UPDATE: v_update(v); break;
      case V_ABOUT: v_about(v); break;
      case V_FAVS: v_favs(v); break;
      case V_LOOK: v_look(v); break;
      case V_FONTS: v_fonts(v); break;
      case V_QUICK: v_quick(v); break;
      case V_HOME: v_home(v); break;
      case V_AVISOS: v_avisos(v); break;
    }
    if (v->page < 0) v->page = 0;
    if (!(v->mod < 0 && v->type == V_DESK)) footer(v->page);
    if (v->page == before) break;
  }
  if (escDark) InvertArea(0, 0, SW, SH);
}
static void draw(int full) {
  static int nfull;
  render(); cfg_load();
  /* full: 1 = canvi de pantalla (refresc complet cada cfgFull pantalles), 2 = sempre complet */
  if (full == 2 || (full && (cfgFull <= 1 || ++nfull >= cfgFull))) { FullUpdate(); nfull = 0; } else SoftUpdate();
  if (pendMsg) { const char *m = pendMsg; pendMsg = NULL; Message(ICON_INFORMATION, "Escriptori", m, 4000); }
}
/* Tic d'un minut a l'escriptori: només es refresca la barra d'estat i, si n'hi ha, la zona del rellotge (no tota la pantalla). */
static int desk_tick(void) {
  View *v = &stack[depth - 1];
  if (v->mod >= 0 || v->type != V_DESK) return 0;
  { int oS[NSYS], oX[40], oW[40], oN = sbN, i, lay = 0, x0 = SW, x1 = 0;
    memcpy(oS, sbSig, sizeof(oS)); memcpy(oX, sbX, sizeof(oX)); memcpy(oW, sbW, sizeof(oW));
    render();
    if (oN != sbN) lay = 1;
    for (i = 0; i < 40 && !lay; i++) if (oX[i] != sbX[i] || oW[i] != sbW[i]) lay = 1;
    if (lay) { x0 = 0; x1 = SW; }
    else for (i = 0; i < NSYS; i++) if (oS[i] != sbSig[i] && sbW[i] > 0) { if (sbX[i] - 3 < x0) x0 = sbX[i] - 3; if (sbX[i] + sbW[i] + 3 > x1) x1 = sbX[i] + sbW[i] + 3; }
    if (x1 > x0) PartialUpdate(x0 < 0 ? 0 : x0, 0, (x1 > SW ? SW : x1) - (x0 < 0 ? 0 : x0), SB); }
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
static const char *h_front(void) { int m = depth > 0 ? stack[depth - 1].mod : -1; return m >= 0 && m < nmods ? mods[m].id : ""; }

static void open_module(int i) {
  View *nv;
  if (i < 0 || i >= nmods) return;
  nv = push_view(i, 0, 0, 0, 0, NULL);
  if (mods[i].m->fn_open) mods[i].m->fn_open(nv);
}
static void host_fonts(void) { host.fTitle = fTitle; host.fBold = fBold; host.fText = fText; host.fSmall = fSmall; host.fBig = fBig; host.fHuge = fHuge; }
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
      else if (arg == nmods + 1) { push_view(-1, V_SYSTEM, 0, 0, 0, NULL); draw(1); }   /* Configuració */
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
    case A_AVISOS: if (!(v->mod < 0 && v->type == V_AVISOS)) { push_view(-1, V_AVISOS, 0, 0, 0, NULL); draw(1); } break;
    case A_AV_ONE: if (arg >= 0 && arg < nAv) { if (!av[arg].read) av[arg].read = 1; else { memmove(&av[arg], &av[arg + 1], sizeof(Aviso) * (size_t)(nAv - arg - 1)); nAv--; } avisos_save(); draw(0); } break;
    case A_AV_READ: { int i; for (i = 0; i < nAv; i++) av[i].read = 1; avisos_save(); draw(0); } break;
    case A_AV_CLEAR: nAv = 0; avisos_save(); draw(0); break;
    case A_CFG_GO: push_view(-1, arg, 0, 0, 0, NULL); draw(1); break;
    case A_CFG_FULL: cfgFull = cfgFull <= 1 ? 3 : cfgFull == 3 ? 5 : cfgFull == 5 ? 10 : 1; cfg_save(); draw(0); break;
    case A_FAV_TOGGLE: {
      const char *k = app_key(arg); int p = fav_pos(k);
      if (!k) break;
      if (p >= 0) { memmove(favKeys[p], favKeys[p + 1], sizeof(favKeys[0]) * (size_t)(nFav - p - 1)); nFav--; }
      else if (nFav < 4) snprintf(favKeys[nFav++], 64, "%s", k);
      else { say("Ja hi ha 4 preferits. Treu-ne un abans d'afegir-ne un altre."); break; }
      fav_save(); draw(0);
    } break;
    case A_STORE_FILTER: v->a = arg; v->page = 0; draw(0); break;
    case A_ABOUT_UPD: upd_check(); push_view(-1, V_UPDATE, 0, 0, 0, NULL); draw(1); break;
    case A_LOOK_THEME: cfgTheme = arg == 1; cfg_save(); theme_apply(); draw(2); break;
    case A_LOOK_CON: cfgCon = arg == 1; cfg_save(); theme_apply(); draw(2); break;
    case A_LOOK_BG: cfgBg = arg < 0 ? 0 : arg > 2 ? 2 : arg; if (cfgBg == 2) mkdir(WALLDIR, 0777); cfg_save(); theme_apply(); draw(2); break;
    case A_LOOK_SIZE: cfgSize = arg < 0 ? 0 : arg > 3 ? 3 : arg; cfg_save(); fonts_apply(); host_fonts(); draw(2); break;
    case A_FONT_PICK:
      if (arg < 0 || arg > nfam) break;
      snprintf(cfgFont, sizeof(cfgFont), "%s", arg == 0 ? "" : famN[arg - 1]);
      cfg_save(); fonts_apply(); host_fonts(); draw(2);
      break;
    case A_DESK_PREV: if (v->page > 0) { v->page--; draw(0); } break;
    case A_DESK_NEXT: if (v->page < pages - 1) { v->page++; draw(0); } break;
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
    case A_QCLEAN: draw(2); break;
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
      cfg_load(); theme_apply(); fonts_apply();
      host.api = ESC_API; host.SW = SW; host.SH = SH; host.U = U; host.SB = SB;
      host_fonts(); host.pal = escPal; host.icon_fg = PAL(P_FG); host.icon_bg = PAL(P_CARD); host.update = upd_region;
      host.pages = &pages; host.add_hit = add_hit; host.button = button; host.header = header; host.redraw = draw; host.push = h_push; host.pop = h_pop;
      host.say = say; host.ask = ask; host.confirm = confirm; host.net_get = net_get_t; host.notify = avisos_add; host.status_changed = sb_refresh; host.front = h_front; host.on_unload = h_on_unload;
      depth = 1; memset(&stack[0], 0, sizeof(View)); stack[0].type = V_DESK; stack[0].mod = -1;
      mkdir(SYSDIR, 0777); mkdir(PKGDIR, 0777);
      mods_scan(); cat_load(); avisos_load();
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
    case EVT_NET_CONNECTED:
    case EVT_NET_DISCONNECTED:
      sb_refresh();                        /* el Wi-Fi ha canviat: només la peça del Wi-Fi */
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
