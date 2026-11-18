/* Escriptori — sistema base: escriptori, barra d'estat, gestor de paquets i actualització. */
#define _GNU_SOURCE                      /* dladdr */
#include "mod.h"
#include <dlfcn.h>
#include <sys/statvfs.h>
#include <limits.h>

#define APP_BUILD 12
#define APP_VERNAME "2.4"
#define UPD_BASE "https://raw.githubusercontent.com/sarnaloka/pb_launcher/main/"
#define SYSDIR FLASHDIR "/system/escriptori"
#define PKGDIR SYSDIR "/packages"

enum { V_DESK, V_STORE, V_PKG, V_SYSTEM, V_QUICK, V_HOME, V_AVISOS, V_UPDATE, V_ABOUT, V_FAVS, V_LOOK, V_FONTS, V_JOBS, V_OPEN, V_SPLIT, V_SPLITPICK, V_MON };   /* V_SYSTEM = Configuració */
enum { A_NONE, A_BACK, A_HOME, A_PREV, A_NEXT, A_EXIT, A_APP, A_STORE_REFRESH, A_STORE_ALL, A_PKG, A_PKG_INSTALL, A_PKG_REMOVE,
       A_UPD_CHECK, A_UPD_INSTALL, A_WIFI,
       A_QUICK, A_WIDGET, A_QDARK, A_QLIGHT_SET, A_QWIFI, A_QLIGHT_DN, A_QLIGHT_UP, A_QCLEAN, A_QGO, A_HE_TOGGLE, A_HE_UP, A_HE_DN, A_AVISOS, A_AV_ONE, A_AV_READ, A_AV_CLEAR,
       A_CFG_GO, A_CFG_FULL, A_FAV_TOGGLE, A_STORE_FILTER, A_ABOUT_UPD,
       A_LOOK_THEME, A_LOOK_SIZE, A_LOOK_BG, A_LOOK_CON, A_FONT_PICK, A_DESK_PREV, A_DESK_NEXT,
       A_JOB_CANCEL, A_SYNC_TOGGLE, A_SYNC_CHARGE, A_SYNC_NOW, A_OPEN, A_OPEN_CLOSE, A_OPEN_ALL, A_OPEN_SPLIT,
       A_SPLIT_SWAP, A_SPLIT_SIZE, A_SPLIT_CLOSE, A_PICK_TOP, A_PICK_BOT, A_PICK_GO, A_MON_STOP, A_MON_REFRESH };          /* les accions dels paquets comencen a 100 */
typedef struct { int x, y, w, h, act, arg, pane; } Hit;   /* pane: panell de la pantalla dividida (-1 = cap) */
#define MAXSTACK 24
#define MAXHITS 400
static View stack[MAXSTACK];
static int depth;
static Hit hits[MAXHITS];
static int nhits;
static int SW, SH, U, SB;
static ifont *fTitle, *fBold, *fText, *fSmall, *fBig, *fHuge;
static ifont *fSbar;                             /* hora de la barra d'estat */
static int pages = 1;
/* L'actualitzador de la compilació 1 rebutja binaris de menys de 100 kB. Aquest farciment fa que la
   transició 1 -> 0.8 sigui possible; es pot treure quan ja no quedi cap lector amb la compilació 1. */
__attribute__((used)) static const char transition_padding[90000] = { 1 };
static const char *pendMsg;
static void say(const char *m) { pendMsg = m; }

#ifndef SBK
#define SBK 9                                   /* alçada de la barra d'estat, en quarts d'U */
#endif
#include "base_gen.h"
#include "ui.h"

/* ───────────── aparença: ajustos, tema, fonts i fons ─────────────
   system/config/escriptori_config.txt: refresc, tema (0 clar, 1 fosc), mida (0..3), fons (0 cap, 1 gris, 2 imatge),
   contrast (0 suau, 1 fort) i font (família; buit = la del lector). */
static int cfgFull = 1, cfgLoaded, cfgTheme, cfgSize = 1, cfgBg = 1, cfgCon, cfgCharge;   /* cfgCharge: sincronitza només carregant */
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
    else if (!strncmp(ln, "nomescarregant=", 15)) cfgCharge = atoi(ln + 15) == 1;
  }
  fclose(f);
}
static void cfg_save(void) {
  FILE *f = fopen(CFG "escriptori_config.txt", "w");
  if (!f) return;
  fprintf(f, "refresc=%d\ntema=%d\nmida=%d\nfons=%d\ncontrast=%d\nfont=%s\nnomescarregant=%d\n", cfgFull, cfgTheme, cfgSize, cfgBg, cfgCon, cfgFont, cfgCharge);
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
  ifont *old[9]; char r[140], b[140]; int k, i;
  cfg_load(); k = K[cfgSize];
  old[0] = fTitle; old[1] = fBold; old[2] = fText; old[3] = fSmall; old[4] = fBig; old[5] = fHuge; old[6] = fHead; old[7] = fClock; old[8] = fSbar;
  font_names(cfgFont[0] ? EnumFonts() : NULL, cfgFont, r, b, sizeof(r));
  fTitle = font_open(b, DEFAULTFONTB, U * 11 * k / 1000); fBold = font_open(b, DEFAULTFONTB, U * 9 * k / 1000);
  fText = font_open(r, DEFAULTFONT, U * 9 * k / 1000); fSmall = font_open(r, DEFAULTFONT, U * 7 * k / 1000);
  fBig = font_open(b, DEFAULTFONTB, U * 12 * k / 1000); fHuge = font_open(b, DEFAULTFONTB, U * 3);
  fHead = font_open(b, DEFAULTFONTB, U * 14 * (100 + (k - 100) / 2) / 1000); fClock = font_open(b, DEFAULTFONTB, U * 9 / 2);
  fSbar = font_open(r, DEFAULTFONT, U * 62 / 100);
  for (i = 0; i < 9; i++) if (old[i]) CloseFont(old[i]);
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
static int quick_open(void) { return depth >= 2 && stack[depth - 1].mod < 0 && stack[depth - 1].type == V_QUICK; }
/* Actualització parcial (també per als paquets): amb el tema fosc s'inverteix abans la zona redibuixada. */
static void upd_region(int x, int y, int w, int h) { if (escDark) InvertArea(x, y, w, h); PartialUpdate(x, y + escOY, w, h); }
#include "jobs.h"

/* ───────────── paquets carregats ───────────── */
typedef struct { void *dl; Module *m; char id[48]; int split, reload; int (*ev)(int, int, int); } Loaded;
/* Exportacions opcionals d'un paquet (API 7):
   escriptori_split  — pot sortir a la pantalla dividida;
   escriptori_event  — rep els tocs directes (també el multitàctil, EVT_MTSYNC) quan és a primer pla; si torna 1, el sistema no els tracta;
   escriptori_reload — en tancar-lo des d'«Apps obertes», el sistema el descarrega i el torna a carregar (estat net). */
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
  mods[i].dl = dl; mods[i].m = m; snprintf(mods[i].id, sizeof(mods[i].id), "%s", id); mods[i].split = dlsym(dl, "escriptori_split") != NULL; mods[i].reload = dlsym(dl, "escriptori_reload") != NULL;
  *(void **)&mods[i].ev = dlsym(dl, "escriptori_event");
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
static void open_forget(int m);
static void jobs_drop_owner(const char *id);
static void bg_unregister(const char *id);
static void h_busy(const char *id, const char *state);
static void mod_unload(int i) {
  int k;
  if (i < 0 || i >= nmods) return;
  drop_module_views();
  open_forget(i);
  for (k = 0; k < nunl; k++) if (!strcmp(unl[k].id, mods[i].id)) { unl[k].fn(); unl[k] = unl[nunl - 1]; nunl--; break; }
  jobs_drop_owner(mods[i].id); bg_unregister(mods[i].id); h_busy(mods[i].id, NULL);
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
/* ───────────── apps obertes ─────────────
   En tornar a l'inici des d'una app, se'n desa la pila de vistes i una miniatura de la pantalla; en tornar-la a obrir,
   continua on era. L'estat intern del paquet ja es conserva sol (el paquet continua carregat). Es desa pel nom del
   paquet, no per la posició a mods[], perquè la taula es reordena quan s'instal·la un paquet. */
#define MAXOPEN 8
typedef struct { char id[48]; View v[12]; int n; long t; ibitmap *thumb; } OpenApp;
static OpenApp opened[MAXOPEN]; static int nOpen;
static ibitmap *pendThumb; static char pendThumbId[48];
static int open_find(const char *id) { int i; for (i = 0; i < nOpen; i++) if (!strcmp(opened[i].id, id)) return i; return -1; }
static void open_drop(int k) { if (k < 0 || k >= nOpen) return; free(opened[k].thumb); memmove(&opened[k], &opened[k + 1], sizeof(OpenApp) * (size_t)(nOpen - k - 1)); nOpen--; }
static void open_forget(int m) { int k; if (m < 0 || m >= nmods) return; while ((k = open_find(mods[m].id)) >= 0) open_drop(k); if (!strcmp(pendThumbId, mods[m].id)) { free(pendThumb); pendThumb = NULL; pendThumbId[0] = 0; } }
/* Tanca una app oberta. Si el paquet ho demana (escriptori_reload), es torna a carregar perquè la propera vegada comenci net. */
static void open_close(int k) {
  char id[48]; int m;
  if (k < 0 || k >= nOpen) return;
  snprintf(id, sizeof(id), "%s", opened[k].id);
  open_drop(k);
  m = mod_find(id);
  if (m >= 0 && mods[m].reload) { mod_unload(m); mod_load(id); }
}
/* Miniatura d'1/3 de la pantalla (sota la barra) de l'app que es veu ara. */
static void thumb_capture(void) {
  View *v = &stack[depth - 1]; ibitmap *b; int tw, th, x, y;
  if (v->mod < 0 || v->mod >= nmods) return;
  b = BitmapFromScreen(0, SB, SW, SH - SB);
  free(pendThumb); pendThumb = NULL; pendThumbId[0] = 0;
  if (!b) return;
  if (b->depth != 8) { free(b); return; }
  tw = SW / 3; th = (SH - SB) / 3;
  pendThumb = (ibitmap *)malloc(sizeof(ibitmap) + (size_t)tw * th);
  if (pendThumb) {
    pendThumb->width = (unsigned short)tw; pendThumb->height = (unsigned short)th; pendThumb->depth = 8; pendThumb->scanline = (unsigned short)tw;
    for (y = 0; y < th; y++) for (x = 0; x < tw; x++) {
      int sum = 0, a, c;
      for (a = 0; a < 3; a++) for (c = 0; c < 3; c++) sum += b->data[(y * 3 + a) * b->scanline + x * 3 + c];
      pendThumb->data[y * tw + x] = (unsigned char)(escDark ? 255 - sum / 9 : sum / 9);   /* en fosc es desa «sense invertir» */
    }
    snprintf(pendThumbId, sizeof(pendThumbId), "%s", mods[v->mod].id);
  }
  free(b);
}
/* Desa l'app que hi ha a la pila (la primera vista de paquet i les següents del mateix paquet). */
static void open_save_current(void) {
  int k, m, n = 0, slot; View tmp[12];
  for (k = 0; k < depth && stack[k].mod < 0; k++) {}
  if (k >= depth) return;
  m = stack[k].mod;
  if (m < 0 || m >= nmods) return;
  for (; k < depth && n < 12; k++) if (stack[k].mod == m) tmp[n++] = stack[k];
  slot = open_find(mods[m].id);
  if (slot < 0) {
    if (nOpen == MAXOPEN) { int old = 0, i; for (i = 1; i < nOpen; i++) if (opened[i].t < opened[old].t) old = i; open_drop(old); }
    slot = nOpen++; memset(&opened[slot], 0, sizeof(OpenApp));
  }
  snprintf(opened[slot].id, sizeof(opened[slot].id), "%s", mods[m].id);
  memcpy(opened[slot].v, tmp, sizeof(View) * (size_t)n); opened[slot].n = n; opened[slot].t = (long)time(NULL);
  if (pendThumb && !strcmp(pendThumbId, mods[m].id)) { free(opened[slot].thumb); opened[slot].thumb = pendThumb; pendThumb = NULL; pendThumbId[0] = 0; }
}
/* Torna a una app oberta: n'empila les vistes desades. 1 si n'hi havia. */
static int open_resume(int m) {
  int k = open_find(mods[m].id), i;
  if (k < 0) return 0;
  for (i = 0; i < opened[k].n && depth < MAXSTACK; i++) { stack[depth] = opened[k].v[i]; stack[depth].mod = m; depth++; }
  open_drop(k);
  return 1;
}
/* ───────────── pantalla dividida ─────────────
   Dues apps, cadascuna amb la seva pila de vistes. Per dibuixar o atendre un panell, el sistema hi «entra»: SB = 0,
   SH = alçada del panell i escOY/host.oy = posició; les macros de dibuix de mod.h hi sumen el desplaçament, així el
   paquet no canvia. Només hi entren els paquets que exporten «escriptori_split». */
static char splitId[2][48]; static View splitSt[2][12]; static int splitN[2], splitRatio, paneLast;
static int scrSB, scrSH;                                  /* barra i alçada reals de la pantalla */
static int pickTop = -1, pickBot = -1;
static int ctxPages, ctxFootY;
static void split_geom(int p, int *y, int *h) {
  int top = scrSB + U / 4, bar = 2 * U + U / 2, avail = scrSH - top - bar - U / 4, hA;
  hA = splitRatio == 0 ? avail / 2 : splitRatio == 1 ? avail * 2 / 3 : avail / 3;
  if (p == 0) { *y = top; *h = hA; } else { *y = top + hA + bar; *h = avail - hA; }
}
static void pane_enter(int p) {
  int y, h; split_geom(p, &y, &h);
  ctxPages = pages; ctxFootY = footY;
  inPane = p; escOY = y; SB = 0; SH = h; host.oy = y; host.SB = 0; host.SH = h; pages = 1; footY = SH - NAVH - U / 4;
  SetClip(0, y, SW, h);
}
static void pane_leave(void) {
  SB = scrSB; SH = scrSH; host.SB = SB; host.SH = SH; host.oy = 0; escOY = 0; pages = ctxPages; footY = ctxFootY; inPane = -1;
  SetClip(0, 0, SW, SH);
}
static int split_on(void) { return depth > 1 && stack[depth - 1].mod < 0 && stack[depth - 1].type == V_SPLIT; }
/* Desa a «apps obertes» l'app d'un panell. */
static void split_save(int p) {
  int m = mod_find(splitId[p]), slot;
  if (m < 0 || !splitN[p]) return;
  slot = open_find(splitId[p]);
  if (slot < 0) { if (nOpen == MAXOPEN) open_drop(0); slot = nOpen++; memset(&opened[slot], 0, sizeof(OpenApp)); }
  snprintf(opened[slot].id, sizeof(opened[slot].id), "%s", splitId[p]);
  memcpy(opened[slot].v, splitSt[p], sizeof(View) * (size_t)splitN[p]); opened[slot].n = splitN[p]; opened[slot].t = (long)time(NULL);
}
/* Prepara un panell amb l'app m: si era oberta, continua on era. */
static void split_fill(int p, int m) {
  int k = open_find(mods[m].id);
  snprintf(splitId[p], sizeof(splitId[p]), "%s", mods[m].id);
  if (k >= 0) { memcpy(splitSt[p], opened[k].v, sizeof(View) * (size_t)opened[k].n); splitN[p] = opened[k].n; open_drop(k); return; }
  memset(&splitSt[p][0], 0, sizeof(View)); splitSt[p][0].mod = m; splitN[p] = 1;
  if (mods[m].m->fn_open) { pane_enter(p); mods[m].m->fn_open(&splitSt[p][0]); pane_leave(); }
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
static int pkg_install_data(const Pkg *q, char *b, int len);
/* Catàleg: arg = 1 si és la sincronització automàtica (sense missatges). */
static void store_refresh_done(Job *j) {
  int rc = -1;
  if (j->data && j->len > 0) {
    if (!strchr(j->data, '|')) rc = -2;
    else { mkdir(SYSDIR, 0777); mkdir(PKGDIR, 0777); write_file(PKGDIR "/index.txt", j->data, j->len); cat_load(); pkg_notify(); rc = 0; }
  }
  if (rc && !j->arg) say(rc == -1 ? "No s'ha pogut baixar el catàleg. Comprova el Wi-Fi." : "El catàleg del servidor no és vàlid.");
}
static void store_refresh(int quiet) {
  char url[300];
  snprintf(url, sizeof(url), UPD_BASE "packages/index.txt?t=%ld&r=%d", (long)time(NULL), rand());
  jobs_add(url, "Catàleg de paquets", "", 25, store_refresh_done, quiet, NULL, NULL);
}
/* Instal·lacions: es fan a la cua; en acabar un lot, un sol missatge. */
static int instPend, instOk, instBad, instLast;
static int job_find_url_key(const char *key) {
  int i, r = -1;
  pthread_mutex_lock(&jm);
  for (i = 0; i < MAXJOBS; i++) if ((jobs[i].state == J_QUEUED || jobs[i].state == J_RUNNING) && !strcmp(jobs[i].owner, "@inst") && strstr(jobs[i].url, key)) { r = i; break; }
  pthread_mutex_unlock(&jm);
  return r;
}
static const char *pkg_err(int rc);
static void pkg_install_done(Job *j) {
  int i, rc = -1;
  const char *id = j->title + 0;
  (void)id;
  for (i = 0; i < ncat; i++) {
    char key[80]; snprintf(key, sizeof(key), "packages/%s.pkg?", cat[i].id);
    if (strstr(j->url, key)) { rc = j->data ? pkg_install_data(&cat[i], j->data, j->len) : -1; break; }
  }
  if (rc == 0) instOk++; else { instBad++; instLast = rc; }
  if (instPend > 0) instPend--;
  if (instPend == 0) {
    static char msg[160];
    if (instOk + instBad > 1) { snprintf(msg, sizeof(msg), instBad ? "S'han instal·lat %d paquets; %d han fallat." : "S'han instal·lat %d paquets.", instOk, instBad); say(msg); }
    else if (instBad) say(pkg_err(instLast));
    instOk = instBad = 0;
  }
}
static int pkg_queued(const Pkg *q) { char key[80]; snprintf(key, sizeof(key), "packages/%s.pkg?", q->id); return job_find_url_key(key) >= 0; }
/* counted: el lot ja s'ha comptat abans (Instal·la-ho tot), perquè el missatge final surti un sol cop */
static void pkg_install_c(const Pkg *q, int counted) {
  char url[300], t[100];
  if (pkg_queued(q)) { if (counted && instPend > 0) instPend--; return; }   /* ja és a la cua */
  snprintf(url, sizeof(url), UPD_BASE "packages/%s.pkg?t=%ld&r=%d", q->id, (long)time(NULL), rand());
  snprintf(t, sizeof(t), "%s · versió %d", q->name, q->ver);
  if (!counted) instPend++;
  jobs_add(url, t, "@inst", 180, pkg_install_done, 0, NULL, NULL);
}
static void pkg_install(const Pkg *q) { pkg_install_c(q, 0); }
/* 0 correcte; -1 xarxa; -2 la descàrrega no quadra; -3 no es pot desar; -4 no es pot carregar */
static int pkg_install_data(const Pkg *q, char *b, int len) {
  char path[700], ver[32]; int i;
  if ((long)len != q->size || crc32_buf((const unsigned char *)b, len) != q->crc || len < 1000 || memcmp(b, "\177ELF", 4)) return -2;
  mkdir(SYSDIR, 0777); mkdir(PKGDIR, 0777);
  i = mod_find(q->id);
  if (i >= 0) mod_unload(i);
  snprintf(path, sizeof(path), PKGDIR "/%s.pkg", q->id);
  if (write_file(path, b, len) != 0) { mod_load(q->id); return -3; }
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
static void upd_check_done(Job *j) {
  char *b = j->data, *l[5]; int k;
  updState = -1; updNotes[0] = 0; updName[0] = 0;
  if (!b) { if (!j->arg) say("No s'ha pogut consultar la versió. Comprova el Wi-Fi."); return; }
  l[0] = b;
  for (k = 1; k < 5; k++) { l[k] = l[k - 1] ? strchr(l[k - 1], '\n') : NULL; if (l[k]) *l[k]++ = 0; }
  updBuild = atoi(l[0]);
  if (updBuild <= 0 || !l[1] || !l[2]) { if (!j->arg) say("La informació de versió del servidor no és vàlida."); return; }
  updSize = atol(l[1]); updCrc = strtoul(l[2], NULL, 16);
  if (l[3]) { snprintf(updNotes, sizeof(updNotes), "%s", l[3]); updNotes[strcspn(updNotes, "\r\n")] = 0; }
  if (l[4]) { snprintf(updName, sizeof(updName), "%s", l[4]); updName[strcspn(updName, "\r\n")] = 0; }
  if (!updName[0]) snprintf(updName, sizeof(updName), "%d", updBuild);
  updState = updBuild > APP_BUILD ? 2 : 1;
  if (updState == 2) { char t[120]; snprintf(t, sizeof(t), "Versió %s", updName); avisos_add("Sistema", "Actualització disponible", t); }
}
static void upd_check_q(int quiet) {
  char url[300];
  snprintf(url, sizeof(url), UPD_BASE "version.txt?t=%ld&r=%d", (long)time(NULL), rand());
  jobs_add(url, "Versió de l'Escriptori", "", 25, upd_check_done, quiet, NULL, NULL);
}
static void upd_check(void) { upd_check_q(0); }
static void upd_install_done(Job *j) {
  char *b = j->data; int len = j->len;
  if (!b) { say("No s'ha pogut descarregar l'actualització."); return; }
  if ((long)len != updSize || crc32_buf((const unsigned char *)b, len) != updCrc || len < 20000 || memcmp(b, "\177ELF", 4)) {
    say("La descàrrega no coincideix amb la versió publicada. No s'ha canviat res."); return;
  }
  if (write_file(selfPath, b, len) != 0) { say("No s'ha pogut substituir l'aplicació. No s'ha canviat res."); return; }
  chmod(selfPath, 0755);
  updState = 3;
}
static void upd_install(void) {
  char url[300], t[80];
  snprintf(url, sizeof(url), UPD_BASE "Escriptori.app?t=%ld&r=%d", (long)time(NULL), rand());
  snprintf(t, sizeof(t), "Escriptori %s", updName);
  jobs_add(url, t, "", 180, upd_install_done, 0, NULL, NULL);
}

/* ───────────── sincronització automàtica ─────────────
   Cada tasca (el catàleg del sistema i les que registren els paquets amb Host.bg_register) té un interval. Cada minut,
   si el Wi-Fi ja està connectat (no es desperta mai sol) i, si cal, el lector carrega, es llancen les que toquen.
   system/config/escriptori_sync.txt: «id|activa|darrera vegada». */
typedef struct { char id[48], title[64]; int every, on; long last; void (*start)(void); } BgTask;
#define MAXBG 16
static BgTask bgt[MAXBG]; static int nbg, bgLoaded;
static struct { char id[48]; int on; long last; } bgSaved[MAXBG]; static int nbgSaved;
static void bg_save(void) {
  FILE *f = fopen(CFG "escriptori_sync.txt", "w"); int i;
  if (!f) return;
  for (i = 0; i < nbg; i++) fprintf(f, "%s|%d|%ld\n", bgt[i].id, bgt[i].on, bgt[i].last);
  for (i = 0; i < nbgSaved; i++) { int k, dup = 0; for (k = 0; k < nbg; k++) if (!strcmp(bgt[k].id, bgSaved[i].id)) dup = 1; if (!dup) fprintf(f, "%s|%d|%ld\n", bgSaved[i].id, bgSaved[i].on, bgSaved[i].last); }
  fclose(f);
}
static void bg_load(void) {
  FILE *f; char ln[160];
  if (bgLoaded) return;
  bgLoaded = 1;
  if (!(f = fopen(CFG "escriptori_sync.txt", "r"))) return;
  while (nbgSaved < MAXBG && fgets(ln, sizeof(ln), f)) {
    char *a = strchr(ln, '|'), *b2 = a ? strchr(a + 1, '|') : NULL;
    if (!a || !b2) continue;
    *a = 0; snprintf(bgSaved[nbgSaved].id, sizeof(bgSaved[0].id), "%s", ln); bgSaved[nbgSaved].on = atoi(a + 1); bgSaved[nbgSaved].last = atol(b2 + 1); nbgSaved++;
  }
  fclose(f);
}
static void bg_register(const char *id, const char *title, int every, void (*start)(void)) {
  int i;
  if (!id || !start) return;
  bg_load();
  for (i = 0; i < nbg && strcmp(bgt[i].id, id); i++) {}
  if (i == nbg) { if (nbg >= MAXBG) return; nbg++; memset(&bgt[i], 0, sizeof(BgTask)); bgt[i].on = 1; { int k; for (k = 0; k < nbgSaved; k++) if (!strcmp(bgSaved[k].id, id)) { bgt[i].on = bgSaved[k].on; bgt[i].last = bgSaved[k].last; } } }
  snprintf(bgt[i].id, sizeof(bgt[i].id), "%s", id); snprintf(bgt[i].title, sizeof(bgt[i].title), "%s", title ? title : id);
  bgt[i].every = every < 5 ? 5 : every; bgt[i].start = start;
}
static void bg_unregister(const char *id) {           /* el paquet es descarrega: es guarda l'estat, però sense funció */
  int i;
  for (i = 0; i < nbg; i++) if (!strcmp(bgt[i].id, id)) {
    int k; for (k = 0; k < nbgSaved && strcmp(bgSaved[k].id, id); k++) {}
    if (k == nbgSaved && nbgSaved < MAXBG) nbgSaved++;
    if (k < MAXBG) { snprintf(bgSaved[k].id, sizeof(bgSaved[k].id), "%s", id); bgSaved[k].on = bgt[i].on; bgSaved[k].last = bgt[i].last; }
    memmove(&bgt[i], &bgt[i + 1], sizeof(BgTask) * (size_t)(nbg - i - 1)); nbg--; i--;
  }
}
static void bg_run(int i) { bgt[i].last = (long)time(NULL); bg_save(); if (bgt[i].start) bgt[i].start(); }
static void bg_tick(void) {
  int i, ran = 0; long now = (long)time(NULL);
  if (!(QueryNetwork() & NET_CONNECTED)) return;
  if (cfgCharge && !IsCharging()) return;
  for (i = 0; i < nbg; i++) if (bgt[i].on && bgt[i].start && now - bgt[i].last >= (long)bgt[i].every * 60) { bg_run(i); ran++; }
  (void)ran;
}
static void bg_now(void) { int i; for (i = 0; i < nbg; i++) if (bgt[i].on && bgt[i].start) bg_run(i); }
static void bg_catalog(void) { store_refresh(1); }

/* ───────────── feina en curs dels paquets (Host.busy) ───────────── */
static struct { char id[48], state[100]; } busyT[16]; static int nbusy;
static void h_busy(const char *id, const char *state) {
  int i;
  if (!id) return;
  for (i = 0; i < nbusy && strcmp(busyT[i].id, id); i++) {}
  if (!state) { if (i < nbusy) { busyT[i] = busyT[nbusy - 1]; nbusy--; } return; }
  if (i == nbusy) { if (nbusy >= 16) return; nbusy++; }
  snprintf(busyT[i].id, sizeof(busyT[i].id), "%s", id); snprintf(busyT[i].state, sizeof(busyT[i].state), "%s", state);
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
/* El tauler baixa des de dalt (estil Kindle) i tapa la barra d'estat; la resta de la pantalla queda enfosquida amb un
   escaquer d'un píxel. Les mides surten del prototip (pantalla de 1072 d'amplada) i s'escalen amb SW. */
#define QK(v) ((v) * SW / 1072)
static int quickH;                             /* alçada del tauler (per a l'escaquer) */
static void quick_btn(int cx, int cy, const char *label, const IcLayer *L, int nl, int on, int badge, int act, int arg) {
  int r = QK(62), s = QK(60), fg = on ? PAL(P_CARD) : PAL(P_FG), bg = on ? PAL(P_FG) : PAL(P_CARD), i; IcLayer M[3];
  rcard(cx - r, cy - r, 2 * r, 2 * r, r, on ? 0 : QK(3), PAL(P_FG), PAL(P_CARD));
  for (i = 0; i < nl && i < 3; i++) { M[i] = L[i]; M[i].col = fg; }
  ic_layers(M, nl, cx - s / 2, cy - s / 2, s, 1.5, bg);
  if (badge > 0) {
    char t[8]; int br = QK(22), bx = cx + QK(44), by = cy - QK(44);
    snprintf(t, sizeof(t), "%d", badge > 99 ? 99 : badge);
    rfill(bx - br, by - br, 2 * br, 2 * br, br, PAL(P_FG));
    SetFont(fSmall, PAL(P_CARD)); DrawTextRect(bx - br, by - br, 2 * br, 2 * br, t, ALIGN_CENTER | VALIGN_MIDDLE);
  }
  SetFont(fSmall, PAL(P_FG)); DrawTextRect(cx - QK(134), cy + r + QK(8), QK(268), QK(48), label, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
  add_hit(cx - QK(134), cy - r - QK(10), QK(268), 2 * r + QK(70), act, arg);
}
static void v_quick(View *v) {
  static const char *WD[7] = { "diumenge", "dilluns", "dimarts", "dimecres", "dijous", "divendres", "dissabte" };
  static const char *MO[12] = { "de gener", "de febrer", "de març", "d'abril", "de maig", "de juny", "de juliol", "d'agost", "de setembre", "d'octubre", "de novembre", "de desembre" };
  int net = QueryNetwork(), lv = light_get(), nu = avisos_unread(), cx[4], i, ph = QK(820), fg = PAL(P_FG), card = PAL(P_CARD), nj = jobs_count(J_QUEUED) + jobs_count(J_RUNNING);
  char t[120]; time_t now = time(NULL); struct tm *tm = localtime(&now);
  (void)v;
  quickH = ph;
  for (i = 0; i < 4; i++) cx[i] = SW * (2 * i + 1) / 8;
  add_hit(0, 0, SW, SH, A_BACK, 0);                          /* fora del tauler: es tanca */
  add_hit(0, 0, SW, ph, A_NONE, 0);
  rcard(0, -QK(40), SW, ph + QK(40), QK(30), QK(3), fg, card);
  /* capçalera: hora i data; a la dreta, bateria i sortir */
  snprintf(t, sizeof(t), "%d:%02d · %s, %d %s", tm->tm_hour, tm->tm_min, WD[tm->tm_wday], tm->tm_mday, MO[tm->tm_mon]);
  SetFont(fText, fg); DrawTextRect(QK(56), QK(40), SW - QK(400), QK(64), t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  snprintf(t, sizeof(t), "%d%%", batt_pct());
  DrawTextRect(QK(880) - QK(240), QK(40), QK(240), QK(64), t, ALIGN_RIGHT | VALIGN_MIDDLE);
  batt_icon(QK(892), QK(48), QK(48), fg, card);
  ic_path(IC_POWER, QK(976), QK(50), QK(44), 1.5, fg, card);
  add_hit(QK(950), 0, SW - QK(950), QK(130), A_EXIT, 0);
  /* botons rodons: dues files de quatre */
  { IcLayer L[3] = { { IC_WIFI, 0, 0 } };
    quick_btn(cx[0], QK(215), "Wi-Fi", L, 1, (net & NET_WIFIREADY) != 0, 0, A_QWIFI, 0); }
  { IcLayer L[3] = { { IC_CONTRAST, 0, 0 }, { IC_CONTRAST_F, 0, 1 } };
    quick_btn(cx[1], QK(215), "Mode fosc", L, 2, cfgTheme == 1, 0, A_QDARK, 0); }
  { IcLayer L[3] = { { IC_SYNC, 0, 0 } };
    quick_btn(cx[2], QK(215), "Sincronitza", L, 1, nj > 0, 0, A_SYNC_NOW, 0); }
  { IcLayer L[3] = { { IC_SLIDERS, 0, 0 } };
    quick_btn(cx[3], QK(215), "Configuració", L, 1, 0, 0, A_QGO, 0); }
  { IcLayer L[3] = { { IC_BELL, 0, 0 } };
    quick_btn(cx[0], QK(430), "Avisos", L, 1, 0, nu, A_QGO, 3); }
  { IcLayer L[3] = { { IC_APPS, 0, 0 } };
    quick_btn(cx[1], QK(430), "Apps obertes", L, 1, 0, nOpen, A_QGO, 4); }
  { IcLayer L[3] = { { IC_PULSE, 0, 0 } };
    quick_btn(cx[2], QK(430), "Monitor", L, 1, 0, 0, A_QGO, 5); }
  { IcLayer L[3] = { { IC_CLEAN, 0, 0 } };
    quick_btn(cx[3], QK(430), "Neteja", L, 1, 0, 0, A_QCLEAN, 0); }
  FillArea(QK(56), QK(580), SW - 2 * QK(56), QK(2) > 1 ? QK(2) : 1, PAL(P_LINE));
  /* llum: lliscador amb el valor a sobre del botó; − i + als extrems */
  SetFont(fSmall, fg); DrawTextRect(QK(56), QK(600), QK(400), QK(52), "Llum", ALIGN_LEFT | VALIGN_MIDDLE);
  if (lv >= 0) {
    int x0 = QK(120), x1 = QK(920), ty = QK(698), kx = x0 + (x1 - x0) * lv / 100, th = QK(6) > 2 ? QK(6) : 2, kr = QK(26);
    SetFont(fTitle, fg);
    DrawTextRect(QK(30), ty - QK(40), QK(80), QK(80), "−", ALIGN_CENTER | VALIGN_MIDDLE);
    DrawTextRect(QK(932), ty - QK(40), QK(80), QK(80), "+", ALIGN_CENTER | VALIGN_MIDDLE);
    rfill(x0, ty - th / 2, x1 - x0, th, th / 2, PAL(P_SHADOW));
    if (kx > x0) rfill(x0, ty - th / 2, kx - x0, th, th / 2, fg);
    snprintf(t, sizeof(t), "%d", lv);
    SetFont(fSmall, fg); DrawTextRect(kx - QK(60), ty - QK(76), QK(120), QK(40), t, ALIGN_CENTER | VALIGN_MIDDLE);
    rcard(kx - kr, ty - kr, 2 * kr, 2 * kr, kr, QK(3), fg, card);
    add_hit(0, ty - QK(50), QK(110), QK(100), A_QLIGHT_DN, 0);
    add_hit(QK(930), ty - QK(50), SW - QK(930), QK(100), A_QLIGHT_UP, 0);
    for (i = 0; i <= 10; i++) { int hx = x0 + (x1 - x0) * i / 10; add_hit(hx - (x1 - x0) / 20, ty - QK(50), (x1 - x0) / 10, QK(100), A_QLIGHT_SET, i * 10); }
  } else { SetFont(fSmall, PAL(P_SUB)); DrawTextRect(QK(56), QK(660), SW - 2 * QK(56), QK(70), "Aquest lector no informa de la llum frontal.", ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  /* xebró per tancar */
  { int t2 = QK(4) > 2 ? QK(4) : 2;
    thick(QK(500), QK(785), QK(536), QK(763), t2, fg); thick(QK(536), QK(763), QK(572), QK(785), t2, fg);
    add_hit(QK(400), QK(740), QK(272), ph - QK(740), A_BACK, 0); }
}
/* Escaquer d'un píxel (els píxels negres alternats): enfosqueix sense grisos. Al lector s'escriu directament al llenç. */
static void checker(int x, int y, int w, int h) {
  int px, py;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SW) w = SW - x;
  if (y + h > SH) h = SH - y;
  if (w <= 0 || h <= 0) return;
#ifdef ESC_SIM
  SimChecker(x, y, w, h); (void)px; (void)py;
#else
  { icanvas *c = GetCanvas();
    if (c && c->addr && (c->depth == 8 || c->depth == 16 || c->depth == 24 || c->depth == 32) && x + w <= c->width && y + h <= c->height) {
      int bpp = c->depth / 8;
      for (py = y; py < y + h; py++) { unsigned char *row = c->addr + (size_t)py * c->scanline; for (px = x + ((x + py + 1) & 1); px < x + w; px += 2) memset(row + (size_t)px * bpp, 0, (size_t)bpp); }
      return; }
    for (py = y; py < y + h; py++) for (px = x + ((x + py + 1) & 1); px < x + w; px += 2) DrawPixel(px, py, BLACK); }
#endif
}
/* Enfosqueix tot el que queda fora del tauler, cantonades arrodonides incloses. */
static void quick_dim(void) {
  int r = QK(30), k, ph = quickH;
  for (k = 0; k < r; k++) {
    double dy = k + 0.5; int inset = r - (int)(sqrt((double)r * r - dy * dy) + 0.5);
    checker(0, ph - r + k, inset, 1); checker(SW - inset, ph - r + k, inset, 1);
  }
  checker(0, ph, SW, SH - ph);
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
  int y = TOP - U / 4, rh = 2 * U, g = U / 4, net = QueryNetwork(), lv = light_get(), nu = avisos_unread(), bs, yy; char t[80];
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
  yy = cfg_group(y, "SISTEMA", 4, rh);
  if (updState == 2) snprintf(t, sizeof(t), "Versió %s disponible", updName); else if (updState == 1) snprintf(t, sizeof(t), "Al dia"); else if (updState == 3) snprintf(t, sizeof(t), "Instal·lada: reobre l'Escriptori"); else snprintf(t, sizeof(t), "Versió %s", APP_VERNAME);
  cfg_row(yy, rh, 1, "Actualització", t, CR_CHEV, A_CFG_GO, V_UPDATE);
  row_icon(IC_DOWNLOAD, yy, rh);
  { const char *m = GetDeviceModel(); snprintf(t, sizeof(t), "%s", m && m[0] ? m : ""); }
  cfg_row(yy + rh, rh, 0, "Sobre el dispositiu", t, CR_CHEV, A_CFG_GO, V_ABOUT);
  row_icon(IC_INFO, yy + rh, rh);
  { int nj = jobs_count(J_QUEUED) + jobs_count(J_RUNNING); if (nj) snprintf(t, sizeof(t), nj == 1 ? "%d descàrrega" : "%d descàrregues", nj); else snprintf(t, sizeof(t), "%s", cfgCharge ? "Només carregant" : "Sincronització activa"); }
  cfg_row(yy + 2 * rh, rh, 0, "Feina de fons", t, CR_CHEV, A_CFG_GO, V_JOBS);
  row_icon(IC_REFRESH, yy + 2 * rh, rh);
  cfg_row(yy + 3 * rh, rh, 0, "Monitor de processos", "", CR_CHEV, A_CFG_GO, V_MON);
  row_icon(IC_BARS, yy + 3 * rh, rh);
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
/* ───────────── Apps obertes ───────────── */
static void ago(char *o, int sz, long t);
static void draw_thumb(const ibitmap *b, int x, int y, int w, int h) {
  int cw = b->width < w ? b->width : w, ch = b->height < h ? b->height : h, r; ibitmap *c;
  c = (ibitmap *)malloc(sizeof(ibitmap) + (size_t)cw * ch);
  if (!c) return;
  c->width = (unsigned short)cw; c->height = (unsigned short)ch; c->depth = 8; c->scanline = (unsigned short)cw;
  for (r = 0; r < ch; r++) memcpy(c->data + r * cw, b->data + r * b->scanline, (size_t)cw);
  DrawBitmap(x + (w - cw) / 2, y, c);
  free(c);
}
static void v_open(View *v) {
  int gap = U / 2, cw = (SW - U - gap) / 2, ch = (SH - TOP - 2 * U - U / 2 - U - gap) / 2, per = 4, i, k, ty, bh = 2 * U + U / 4, bw = (SW - U - U / 3) / 2; char sub[60], t[120], a[40];
  snprintf(sub, sizeof(sub), nOpen == 1 ? "%d oberta" : "%d obertes", nOpen);
  header("Apps obertes", sub);
  pages = nOpen ? (nOpen + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!nOpen) {
    ui_card(U / 2, TOP, SW - U, 4 * U, UI_LINE);
    SetFont(fText, PAL(P_SUB)); DrawTextRect(U + U / 4, TOP, SW - 3 * U, 4 * U, "No hi ha cap app oberta. Les que facis servir quedaran aquí quan tornis a l'inici.", ALIGN_LEFT | VALIGN_MIDDLE);
  }
  for (k = 0, i = v->page * per; i < nOpen && k < per; i++, k++) {
    int x = U / 2 + (k % 2) * (cw + gap), y = TOP + (k / 2) * (ch + gap), m = mod_find(opened[i].id), ph = ch - 3 * U, T = 2 * U, d = U * 7 / 4;
    ui_card(x, y, cw, ch, UI_LINE);
    rfill(x + U / 4, y + U / 4, cw - U / 2, ph - U / 4, U / 2, PAL(P_TILE));
    if (opened[i].thumb) draw_thumb(opened[i].thumb, x + U / 4 + U / 4, y + U / 2, cw - U, ph - U);
    else if (m >= 0) app_icon(m, x + (cw - 3 * U) / 2, y + (ph - 3 * U) / 2, 3 * U, 70, PAL(P_SUB), PAL(P_TILE));
    add_hit(x, y, cw, ch, A_OPEN, i);
    ty = y + ph + U / 4;
    rfill(x + U / 3, ty + U / 4, T, T, T * 35 / 100, PAL(P_TILE));
    if (m >= 0) app_icon(m, x + U / 3, ty + U / 4, T, 72, PAL(P_FG), PAL(P_TILE));
    SetFont(fBold, PAL(P_FG)); DrawTextRect(x + U / 3 + T + U / 3, ty, cw - T - d - U * 3 / 2, U + U / 4, m >= 0 ? mods[m].m->name : opened[i].id, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    ago(a, sizeof(a), opened[i].t);
    { void (*rs)(char *, int) = m >= 0 ? (void (*)(char *, int))dlsym(mods[m].dl, "escriptori_resume") : NULL; char r[80] = ""; if (rs) rs(r, sizeof(r)); if (r[0]) snprintf(t, sizeof(t), "%s", r); else snprintf(t, sizeof(t), "Oberta %s", a); }
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + U / 3 + T + U / 3, ty + U + U / 4, cw - T - d - U * 3 / 2, U, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    round_icon(x + cw - U / 3 - d, ty + U / 2, d, 0, IC_CLOSE);
    add_hit(x + cw - d - U, ty, d + U, 3 * U, A_OPEN_CLOSE, i);
  }
  ty = SH - NAVH - U / 2 - U / 4 - bh - (pages > 1 ? 0 : -NAVH - U / 4);
  if (nOpen) button(U / 2, ty, bw, bh, "Tanca-les totes", 0, A_OPEN_ALL, 0);
  button(nOpen ? U / 2 + bw + U / 3 : U / 2, ty, nOpen ? bw : SW - U, bh, "Pantalla dividida", 1, A_OPEN_SPLIT, 0);
}
/* ───────────── Pantalla dividida ───────────── */
static void v_split(View *v) {
  static const char *SZ[3] = { "Meitat i meitat", "Més gran a dalt", "Més gran a baix" };
  int p, y, h, bh2 = 2 * U, bw = (SW - U - 2 * (U / 3)) / 3, by;
  (void)v;
  for (p = 0; p < 2; p++) {
    int m = mod_find(splitId[p]);
    split_geom(p, &y, &h);
    rcard(U / 4, y, SW - U / 2, h, U, 2, PAL(P_LINE), PAL(P_BG));
    if (m < 0 || !splitN[p]) { SetFont(fText, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, h, "Aquesta app ja no està instal·lada.", ALIGN_CENTER | VALIGN_MIDDLE); continue; }
    pane_enter(p);
    { View *pv = &splitSt[p][splitN[p] - 1]; pv->mod = m;
      mods[m].m->fn_draw(pv);
      if (pv->page >= pages) pv->page = pages > 0 ? pages - 1 : 0;
      footer(pv->page); }
    pane_leave();
  }
  split_geom(0, &y, &h);
  by = y + h + (2 * U + U / 2 - bh2) / 2;
  button(U / 2, by, bw, bh2, "Intercanvia", 1, A_SPLIT_SWAP, 0);
  button(U / 2 + bw + U / 3, by, bw, bh2, SZ[splitRatio], 0, A_SPLIT_SIZE, 0);
  button(U / 2 + 2 * (bw + U / 3), by, bw, bh2, "Tanca la divisió", 0, A_SPLIT_CLOSE, 0);
}
static void v_splitpick(View *v) {
  int y = TOP, rh = 3 * U, n = 0, i, k, T = 2 * U + U / 4, pw = 3 * U + U / 2, ph = 2 * U, bh = 2 * U + U / 2, ok; char sub[100];
  (void)v;
  for (i = 0; i < nmods; i++) if (mods[i].split) n++;
  if (pickTop < 0 || pickTop >= nmods || !mods[pickTop].split) { pickTop = -1; for (i = 0; i < nmods; i++) if (mods[i].split && (pickTop < 0 || open_find(mods[i].id) >= 0)) { pickTop = i; if (open_find(mods[i].id) >= 0) break; } }
  if (pickBot < 0 || pickBot >= nmods || !mods[pickBot].split || pickBot == pickTop) { pickBot = -1; for (i = 0; i < nmods; i++) if (mods[i].split && i != pickTop) { pickBot = i; break; } }
  snprintf(sub, sizeof(sub), "Tria l'app de dalt i la de baix");
  header("Pantalla dividida", sub);
  if (n < 2) { SetFont(fText, PAL(P_FG)); DrawTextRect(U / 2, y, SW - U, 4 * U, "Cal tenir instal·lades almenys dues apps que es puguin dividir (Notes, Tasques, Calculadora o Lector).", ALIGN_LEFT | VALIGN_TOP); return; }
  ui_card(U / 2, y, SW - U, n * rh, UI_LINE);
  for (i = 0, k = 0; i < nmods; i++) {
    int ry, tx;
    if (!mods[i].split) continue;
    ry = y + k * rh;
    if (k) FillArea(U, ry, SW - 2 * U, 2, PAL(P_LINE));
    rfill(U, ry + (rh - T) / 2, T, T, T * 35 / 100, PAL(P_TILE)); app_icon(i, U, ry + (rh - T) / 2, T, 72, PAL(P_FG), PAL(P_TILE));
    SetFont(fBold, PAL(P_FG)); DrawTextRect(U + T + U / 2, ry, SW - 4 * U - T - 2 * pw, rh, mods[i].m->name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    tx = SW - U - 2 * pw - U / 3;
    rfill(tx, ry + (rh - ph) / 2, pw, ph, ph / 2, i == pickTop ? PAL(P_ACC) : PAL(P_TILE));
    SetFont(fBold, i == pickTop ? PAL(P_ACCFG) : PAL(P_FG)); DrawTextRect(tx, ry + (rh - ph) / 2, pw, ph, "A dalt", ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(tx - U / 4, ry, pw + U / 3, rh, A_PICK_TOP, i);
    rfill(tx + pw + U / 3, ry + (rh - ph) / 2, pw, ph, ph / 2, i == pickBot ? PAL(P_ACC) : PAL(P_TILE));
    SetFont(fBold, i == pickBot ? PAL(P_ACCFG) : PAL(P_FG)); DrawTextRect(tx + pw + U / 3, ry + (rh - ph) / 2, pw, ph, "A baix", ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(tx + pw + U / 4, ry, pw + U / 2, rh, A_PICK_BOT, i);
    k++;
  }
  y += n * rh + U / 2;
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, 3 * U, "Les altres apps s'obren sempre a pantalla completa. Si una de les triades ja era oberta, continua on era.", ALIGN_LEFT | VALIGN_TOP);
  ok = pickTop >= 0 && pickBot >= 0 && pickTop != pickBot;
  if (ok) button(U / 2, SH - U / 2 - bh, SW - U, bh, "Obre la pantalla dividida", 1, A_PICK_GO, 0);
}
/* Acció dins d'un panell. */
static void split_exit(int keep) {                       /* keep = panell que queda a pantalla completa, -1 cap */
  int p;
  for (p = 0; p < 2; p++) if (p != keep) split_save(p);
  depth = 1; stack[0].page = 0;
  if (keep >= 0) { int m = mod_find(splitId[keep]), i; if (m >= 0) for (i = 0; i < splitN[keep] && depth < MAXSTACK; i++) { stack[depth] = splitSt[keep][i]; stack[depth].mod = m; depth++; } }
  splitN[0] = splitN[1] = 0;
}
static void split_act(int p, int a, int arg) {
  int m = mod_find(splitId[p]); View *pv;
  if (m < 0 || !splitN[p]) return;
  paneLast = p;
  pv = &splitSt[p][splitN[p] - 1]; pv->mod = m;
  if (a == A_HOME) { split_exit(-1); draw(1); return; }
  if (a == A_BACK) {
    int handled;
    pane_enter(p); handled = mods[m].m->fn_back && mods[m].m->fn_back(pv); pane_leave();
    if (handled) { draw(1); return; }
    if (splitN[p] > 1) { splitN[p]--; draw(1); return; }
    split_exit(1 - p); draw(1); return;                   /* enrere a l'arrel: l'altra app queda a pantalla completa */
  }
  if (a == A_PREV) { if (pv->page > 0) { pv->page--; draw(0); } return; }
  if (a == A_NEXT) { pv->page++; draw(0); return; }
  if (a >= 100 && mods[m].m->fn_action) { pane_enter(p); mods[m].m->fn_action(pv, a, arg); pane_leave(); }
}
/* ───────────── Monitor ───────────── */
static long proc_val(const char *file, const char *key) {
  FILE *f = fopen(file, "r"); char ln[256]; long v = -1; size_t k = strlen(key);
  if (!f) return -1;
  while (fgets(ln, sizeof(ln), f)) if (!strncmp(ln, key, k)) { v = atol(ln + k); break; }
  fclose(f);
  return v;
}
static int cpu_sample(long long *tot, long long *idle) {
  FILE *f = fopen("/proc/stat", "r"); long long a[8] = { 0 }; int n;
  if (!f) return 0;
  n = fscanf(f, "cpu %lld %lld %lld %lld %lld %lld %lld %lld", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7]);
  fclose(f);
  if (n < 4) return 0;
  *idle = a[3] + (n > 4 ? a[4] : 0); *tot = a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6] + a[7];
  return 1;
}
static int cpu_pct(void) {                      /* ús del processador des de la mostra anterior (o en 0,3 s) */
  static long long pt, pi; long long t, i;
  if (!pt && cpu_sample(&pt, &pi)) usleep(300000);
  if (!cpu_sample(&t, &i) || t <= pt) return -1;
  { int r = (int)(100 - (i - pi) * 100 / (t - pt)); pt = t; pi = i; return r < 0 ? 0 : r > 100 ? 100 : r; }
}
enum { MR_SYS, MR_VIS, MR_OPEN, MR_BUSY, MR_JOB, MR_SYNC };
static void mon_row(int y, int rh, int first, const char *ic, int dark, const char *name, const char *state, const char *mem, int act, int arg, const char *btn) {
  int T = rh - U / 2 - U / 4, bw = 3 * U + U / 2, bh = U + U / 2;
  if (!first) FillArea(U, y, SW - 2 * U, 2, PAL(P_LINE));
  rfill(U, y + (rh - T) / 2, T, T, T * 35 / 100, dark ? PAL(P_ACC) : PAL(P_TILE));
  ic_path(ic, U + T / 6, y + (rh - T) / 2 + T / 6, T * 2 / 3, 1.8, dark ? PAL(P_ACCFG) : PAL(P_FG), dark ? PAL(P_ACC) : PAL(P_TILE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + T + U / 2, y + U / 6, SW - 4 * U - T - bw - 3 * U, rh / 2, name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U + T + U / 2, y + rh / 2 - U / 8, SW - 4 * U - T - bw, rh / 2, state, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  if (mem && mem[0]) { SetFont(fSmall, PAL(P_FG)); DrawTextRect(SW - U - bw - 3 * U - U / 2, y + U / 6, 3 * U, rh / 2, mem, ALIGN_RIGHT | VALIGN_MIDDLE); }
  if (btn && !btn[0]) {}
  else if (btn) {
    rfill(SW - U - bw, y + (rh - bh) / 2, bw, bh, bh / 2, PAL(P_TILE));
    SetFont(fSmall, PAL(P_FG)); DrawTextRect(SW - U - bw, y + (rh - bh) / 2, bw, bh, btn, ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(SW - U - bw - U / 4, y, bw + U / 2, rh, act, arg);
  } else { SetFont(fSmall, PAL(P_SUB)); DrawTextRect(SW - U - bw, y, bw, rh, "sistema", ALIGN_CENTER | VALIGN_MIDDLE); }
}
static void v_mon(View *v) {
  typedef struct { int kind, idx; char name[64], state[120]; const char *ic; } MRow;
  MRow r[48]; int n = 0, i, k, gy = TOP, gw = (SW - U - 2 * (U / 3)) / 3, gh = 5 * U, y, rh = 3 * U, per, cpu;
  long rss = proc_val("/proc/self/status", "VmRSS:"), thr = proc_val("/proc/self/status", "Threads:"), mt = proc_val("/proc/meminfo", "MemTotal:"), ma = proc_val("/proc/meminfo", "MemAvailable:");
  char a[40], b2[60]; const char *GL[3] = { "Memòria", "Processador", "Bateria" }; char gv[3][24], gn[3][60]; int gp[3];
  if (ma < 0) { long fr = proc_val("/proc/meminfo", "MemFree:"), ca = proc_val("/proc/meminfo", "Cached:"); ma = fr >= 0 ? fr + (ca > 0 ? ca : 0) : -1; }
  cpu = cpu_pct();
  header("Monitor", "Què hi ha en marxa ara mateix");
  snprintf(gv[0], 24, rss >= 0 ? "%ld MB" : "—", (rss + 512) / 1024); gp[0] = mt > 0 && ma >= 0 ? (int)((long long)(mt - ma) * 100 / mt) : 0;
  if (mt > 0 && ma >= 0) snprintf(gn[0], 60, "lector: %d%% ocupada", gp[0]); else snprintf(gn[0], 60, "de l'Escriptori");
  snprintf(gv[1], 24, cpu >= 0 ? "%d%%" : "—", cpu); gp[1] = cpu > 0 ? cpu : 0; snprintf(gn[1], 60, "des de l'última mirada");
  snprintf(gv[2], 24, "%d%%", batt_pct()); gp[2] = batt_pct(); snprintf(gn[2], 60, "%s", IsCharging() ? "carregant" : "sense carregar");
  for (i = 0; i < 3; i++) {
    int x = U / 2 + i * (gw + U / 3), bw = gw - U;
    ui_card(x, gy, gw, gh, UI_LINE);
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + U / 2, gy + U / 3, bw, U, GL[i], ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fBig, PAL(P_FG)); DrawTextRect(x + U / 2, gy + U + U / 3, bw, U * 3 / 2, gv[i], ALIGN_LEFT | VALIGN_MIDDLE);
    rfill(x + U / 2, gy + 3 * U, bw, U / 3, U / 6, PAL(P_TILE));
    if (gp[i] * bw / 100 > U / 3) rfill(x + U / 2, gy + 3 * U, gp[i] * bw / 100, U / 3, U / 6, PAL(P_FG));
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + U / 2, gy + 3 * U + U / 2, bw, U + U / 4, gn[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  }
  /* llista */
  memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_SYS; r[n].ic = IC_GRID; snprintf(r[n].name, 64, "Escriptori");
  snprintf(r[n].state, 120, thr > 0 ? "Sistema · %ld fils · %s" : "Sistema", thr, jobsAsync > 0 ? "descàrregues en un fil propi" : "descàrregues al fil principal"); n++;
  for (k = 0; k < depth; k++) if (stack[k].mod >= 0 && stack[k].mod < nmods && (k == 0 || stack[k - 1].mod != stack[k].mod)) {
    memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_VIS; r[n].ic = IC_APPS; snprintf(r[n].name, 64, "%s", mods[stack[k].mod].m->name); snprintf(r[n].state, 120, "Oberta · sota aquesta pantalla"); n++;
  }
  for (k = 0; k < depth; k++) if (stack[k].mod < 0 && stack[k].type == V_SPLIT) { int p; for (p = 0; p < 2 && n < 40; p++) { int m = mod_find(splitId[p]); if (m < 0) continue; memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_VIS; r[n].ic = IC_SPLIT; snprintf(r[n].name, 64, "%s", mods[m].m->name); snprintf(r[n].state, 120, "Pantalla dividida · %s", p ? "a baix" : "a dalt"); n++; } }
  for (k = 0; k < nOpen && n < 40; k++) { int m = mod_find(opened[k].id); memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_OPEN; r[n].idx = k; r[n].ic = IC_APPS; snprintf(r[n].name, 64, "%s", m >= 0 ? mods[m].m->name : opened[k].id); ago(a, sizeof(a), opened[k].t); snprintf(r[n].state, 120, "En pausa · oberta %s", a); n++; }
  for (k = 0; k < nbusy && n < 40; k++) { int m = mod_find(busyT[k].id); memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_BUSY; r[n].idx = k; r[n].ic = IC_PULSE; snprintf(r[n].name, 64, "%s", m >= 0 ? mods[m].m->name : busyT[k].id); snprintf(r[n].state, 120, "Treballant · %s", busyT[k].state); n++; }
  for (k = 0; k < MAXJOBS && n < 40; k++) {
    int st; long dn, tot;
    pthread_mutex_lock(&jm); st = jobs[k].state; dn = jobs[k].done; tot = jobs[k].total; pthread_mutex_unlock(&jm);
    if (st != J_QUEUED && st != J_RUNNING) continue;
    memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_JOB; r[n].idx = k; r[n].ic = IC_DOWNLOAD; snprintf(r[n].name, 64, "%s", jobs[k].title);
    if (st == J_RUNNING && tot > 0) snprintf(r[n].state, 120, "Baixant · %d%%", (int)((long long)dn * 100 / tot)); else snprintf(r[n].state, 120, "%s", st == J_RUNNING ? "Baixant" : "A la cua");
    n++;
  }
  { int on = 0; long nxt = -1; for (k = 0; k < nbg; k++) if (bgt[k].on && bgt[k].start) { long d = bgt[k].last + (long)bgt[k].every * 60 - (long)time(NULL); on++; if (nxt < 0 || d < nxt) nxt = d; }
    memset(&r[n], 0, sizeof(MRow)); r[n].kind = MR_SYNC; r[n].ic = IC_REFRESH; snprintf(r[n].name, 64, "Sincronització automàtica");
    if (!on) snprintf(r[n].state, 120, "Aturada");
    else if (!(QueryNetwork() & NET_CONNECTED)) snprintf(r[n].state, 120, "%d tasques · esperant el Wi-Fi", on);
    else if (nxt <= 60) snprintf(r[n].state, 120, "%d tasques · la propera, ara", on);
    else snprintf(r[n].state, 120, "%d tasques · la propera d'aquí a %ld min", on, nxt / 60);
    n++; }
  y = gy + gh + U / 2;
  snprintf(b2, sizeof(b2), "EN MARXA · %d", n);
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, U + U / 4, b2, ALIGN_LEFT | VALIGN_MIDDLE);
  y += U + U / 4;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = (n + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  { int cnt = n - v->page * per; if (cnt > per) cnt = per; ui_card(U / 2, y, SW - U, cnt * rh, UI_LINE); }
  for (k = v->page * per, i = 0; k < n && i < per; k++, i++) {
    const MRow *q = &r[k]; char mem[24] = "";
    if (q->kind == MR_SYS && rss >= 0) snprintf(mem, sizeof(mem), "%ld MB", (rss + 512) / 1024);
    mon_row(y + i * rh, rh, i == 0, q->ic, q->kind == MR_SYS, q->name, q->state, mem,
            q->kind == MR_OPEN || q->kind == MR_BUSY || q->kind == MR_JOB ? A_MON_STOP : q->kind == MR_SYNC ? A_CFG_GO : A_NONE,
            q->kind == MR_OPEN ? 1000 + q->idx : q->kind == MR_BUSY ? 2000 + q->idx : q->kind == MR_JOB ? 3000 + q->idx : V_JOBS,
            q->kind == MR_OPEN ? "Tanca" : q->kind == MR_BUSY || q->kind == MR_JOB ? "Atura" : q->kind == MR_SYNC ? "Obre" : q->kind == MR_VIS ? "" : NULL);
  }
}
/* ───────────── Feina de fons ───────────── */
static void ago(char *o, int sz, long t) {
  long d = (long)time(NULL) - t;
  if (t <= 0) snprintf(o, (size_t)sz, "encara no");
  else if (d < 90) snprintf(o, (size_t)sz, "fa un moment");
  else if (d < 5400) snprintf(o, (size_t)sz, "fa %ld min", d / 60);
  else if (d < 172800) snprintf(o, (size_t)sz, "fa %ld h", d / 3600);
  else snprintf(o, (size_t)sz, "fa %ld dies", d / 86400);
}
static void every_txt(char *o, int sz, int m) {
  if (m % 1440 == 0) snprintf(o, (size_t)sz, m == 1440 ? "Un cop al dia" : "Cada %d dies", m / 1440);
  else if (m % 60 == 0) snprintf(o, (size_t)sz, m == 60 ? "Cada hora" : "Cada %d hores", m / 60);
  else snprintf(o, (size_t)sz, "Cada %d min", m);
}
static void v_jobs(View *v) {
  int y = TOP, rh = 2 * U + U / 2, jh = 3 * U + U / 2, n = 0, i, k, tw = 2 * U, th = U + U / 4, bh = 2 * U + U / 2, yy; char t[160], a[40];
  (void)v; cfg_load();
  header("Feina de fons", jobsAsync > 0 ? "La pantalla continua responent mentre es baixa" : "Les descàrregues es fan d'una en una");
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, U + U / 4, "DESCÀRREGUES", ALIGN_LEFT | VALIGN_MIDDLE);
  y += U + U / 4;
  pthread_mutex_lock(&jm);
  for (i = 0; i < MAXJOBS; i++) if (jobs[i].state == J_QUEUED || jobs[i].state == J_RUNNING) n++;
  pthread_mutex_unlock(&jm);
  if (n > 5) n = 5;
  ui_card(U / 2, y, SW - U, n ? n * jh : rh, UI_LINE);
  if (!n) { SetFont(fText, PAL(P_SUB)); DrawTextRect(U + U / 4, y, SW - 3 * U, rh, "No s'està baixant res.", ALIGN_LEFT | VALIGN_MIDDLE); y += rh; }
  for (i = 0, k = 0; i < MAXJOBS && k < n; i++) {
    int st, pct = -1, bx = U + U / 4, bw = SW - 2 * U - U / 2 - 6 * U, by; long dn, tot;
    pthread_mutex_lock(&jm); st = jobs[i].state; dn = jobs[i].done; tot = jobs[i].total; pthread_mutex_unlock(&jm);
    if (st != J_QUEUED && st != J_RUNNING) continue;
    if (k) FillArea(U, y, SW - 2 * U, 2, PAL(P_LINE));
    SetFont(fBold, PAL(P_FG)); DrawTextRect(bx, y + U / 4, SW - 3 * U - 6 * U, U + U / 2, jobs[i].title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (st == J_RUNNING && tot > 0) { char d1[24], d2[24]; pct = (int)((long long)dn * 100 / tot); fmt_size(d1, sizeof(d1), dn); fmt_size(d2, sizeof(d2), tot); snprintf(t, sizeof(t), "%s de %s", d1, d2); }
    else snprintf(t, sizeof(t), "%s", st == J_RUNNING ? "baixant…" : "a la cua");
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(SW - U - 6 * U - U / 2, y + U / 4, 6 * U, U + U / 2, t, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
    by = y + 2 * U + U / 4;
    rfill(bx, by, bw, U / 3, U / 6, PAL(P_TILE));
    if (pct >= 0 && pct * bw / 100 > U / 3) rfill(bx, by, pct * bw / 100, U / 3, U / 6, PAL(P_FG));
    rfill(SW - U - 4 * U - U / 4, y + 2 * U - U / 4, 4 * U, U + U / 3, (U + U / 3) / 2, PAL(P_TILE));
    SetFont(fSmall, PAL(P_FG)); DrawTextRect(SW - U - 4 * U - U / 4, y + 2 * U - U / 4, 4 * U, U + U / 3, "Cancel·la", ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(SW - U - 5 * U, y + U + U / 2, 5 * U, 2 * U, A_JOB_CANCEL, i);
    y += jh; k++;
  }
  y += U / 2;
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U, y, SW - 2 * U, U + U / 4, "SINCRONITZACIÓ AUTOMÀTICA", ALIGN_LEFT | VALIGN_MIDDLE);
  y += U + U / 4;
  ui_card(U / 2, y, SW - U, (nbg + 1) * rh, UI_LINE);
  yy = y;
  for (i = 0; i < nbg; i++) {
    if (i) FillArea(U, yy, SW - 2 * U, 2, PAL(P_LINE));
    every_txt(t, sizeof(t), bgt[i].every); ago(a, sizeof(a), bgt[i].last);
    { size_t l = strlen(t); snprintf(t + l, sizeof(t) - l, " · darrera: %s", a); }
    SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy + U / 4, SW - 4 * U - tw, U + U / 4, bgt[i].title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U + U / 4, yy + U + U / 2, SW - 4 * U - tw, U, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    ui_toggle(SW - U - tw, yy + (rh - th) / 2, tw, th, bgt[i].on);
    add_hit(U / 2, yy, SW - U, rh, A_SYNC_TOGGLE, i);
    yy += rh;
  }
  FillArea(U, yy, SW - 2 * U, 2, PAL(P_LINE));
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, yy + U / 4, SW - 4 * U - tw, U + U / 4, "Només carregant", ALIGN_LEFT | VALIGN_MIDDLE);
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(U + U / 4, yy + U + U / 2, SW - 4 * U - tw, U, "Sincronitza només amb el lector endollat", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  ui_toggle(SW - U - tw, yy + (rh - th) / 2, tw, th, cfgCharge);
  add_hit(U / 2, yy, SW - U, rh, A_SYNC_CHARGE, 0);
  y = yy + rh + U / 2;
  SetFont(fSmall, PAL(P_SUB));
  DrawTextRect(U, y, SW - 2 * U, 3 * U, "La sincronització només es fa amb el Wi-Fi ja connectat (no l'engega sola) i amb l'Escriptori obert. Quan el lector s'adorm, tot s'atura.", ALIGN_LEFT | VALIGN_TOP);
  button(U / 2, SH - U / 2 - bh, SW - U, bh, "Sincronitza ara", 1, A_SYNC_NOW, 0);
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
static void render_view(View *v);
static void render(void) {
  View *v = &stack[depth - 1];
  int pass;
  netAct = 0;                              /* qualsevol redibuixat completa l'indicador de xarxa */
  if (v->mod >= nmods) { depth = 1; v = &stack[0]; }
  for (pass = 0; pass < 2; pass++) {
    int before = v->page;
    ClearScreen(); FillArea(0, 0, SW, SH, PAL(P_BG)); nhits = 0; pages = 1; footY = SH - NAVH - U / 4; wallOn = 0;
    statusbar(1);
    if (v->mod < 0 && v->type == V_QUICK && depth >= 2) {   /* tauler ràpid: primer la pantalla de sota, sense tocs */
      View *u = &stack[depth - 2];
      depth--; render_view(u); if (u->page < 0) u->page = 0; if (!(u->mod < 0 && (u->type == V_DESK || u->type == V_SPLIT))) footer(u->page); depth++;
      nhits = 0; v_quick(v);
      break;
    }
    render_view(v);
    if (v->page < 0) v->page = 0;
    if (!(v->mod < 0 && (v->type == V_DESK || v->type == V_SPLIT))) footer(v->page);
    if (v->page == before) break;
  }
  if (escDark) InvertArea(0, 0, SW, SH);
  if (quick_open()) quick_dim();
}
static void render_view(View *v) {
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
      case V_JOBS: v_jobs(v); break;
      case V_OPEN: v_open(v); break;
      case V_SPLIT: v_split(v); break;
      case V_SPLITPICK: v_splitpick(v); break;
      case V_MON: v_mon(v); break;
      case V_QUICK: v_quick(v); break;
      case V_HOME: v_home(v); break;
      case V_AVISOS: v_avisos(v); break;
    }
}
static void draw(int full) {
  static int nfull; int inp = inPane;
  if (inp >= 0) pane_leave();
  render(); cfg_load();
  /* full: 1 = canvi de pantalla (refresc complet cada cfgFull pantalles), 2 = sempre complet */
  if (full == 2 || (full && (cfgFull <= 1 || ++nfull >= cfgFull))) { FullUpdate(); nfull = 0; } else SoftUpdate();
  if (pendMsg) { const char *m = pendMsg; pendMsg = NULL; Message(ICON_INFORMATION, "Escriptori", m, 4000); }
  if (inp >= 0) pane_enter(inp);
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
static void h_push(int type, int a, int b, int sel, const char *term) {
  int p = inPane >= 0 ? inPane : split_on() ? paneLast : -1;
  if (p >= 0) {
    if (splitN[p] < 12) { View *nv = &splitSt[p][splitN[p]++]; memset(nv, 0, sizeof(*nv)); nv->mod = mod_find(splitId[p]); nv->type = type; nv->a = a; nv->b = b; nv->sel = sel; if (term) strncpy(nv->term, term, sizeof(nv->term) - 1); }
    draw(1); return;
  }
  push_view(stack[depth - 1].mod, type, a, b, sel, term); draw(1);
}
static void h_pop(void) { int p = inPane >= 0 ? inPane : split_on() ? paneLast : -1; if (p >= 0) { if (splitN[p] > 1) splitN[p]--; return; } if (depth > 1) depth--; }
static const char *h_front(void) {
  int m;
  if (inPane >= 0) return splitId[inPane];
  if (split_on()) return splitId[paneLast];
  m = depth > 0 ? stack[depth - 1].mod : -1; return m >= 0 && m < nmods ? mods[m].id : "";
}

/* Paquet al qual pertany una adreça de codi (per saber de qui és una descàrrega). */
static int mod_of_ptr(void *p) {
  Dl_info a, b; int i;
  if (!p || !dladdr(p, &a)) return -1;
  for (i = 0; i < nmods; i++) if (dladdr((void *)mods[i].m, &b) && b.dli_fbase == a.dli_fbase) return i;
  return -1;
}
static int h_fetch(const char *url, const char *title, void (*cb)(void *, char *, int), void *ctx) {
  int m = mod_of_ptr(*(void **)&cb);
  return jobs_add(url, title ? title : "Descàrrega", m >= 0 ? mods[m].id : "", 60, NULL, 0, cb, ctx) >= 0 ? 0 : -1;
}
static void open_module(int i) {
  View *nv;
  if (i < 0 || i >= nmods) return;
  if (open_resume(i)) return;
  nv = push_view(i, 0, 0, 0, 0, NULL);
  if (mods[i].m->fn_open) mods[i].m->fn_open(nv);
}
static void host_fonts(void) { host.fTitle = fTitle; host.fBold = fBold; host.fText = fText; host.fSmall = fSmall; host.fBig = fBig; host.fHuge = fHuge; }
static void act(int a, int arg) {
  View *v = &stack[depth - 1];
  if (a >= 100) { if (v->mod >= 0 && v->mod < nmods && mods[v->mod].m->fn_action) mods[v->mod].m->fn_action(v, a, arg); return; }
  switch (a) {
    case A_BACK:
      if (v->mod >= 0 && mods[v->mod].m->fn_back && mods[v->mod].m->fn_back(v)) draw(1);
      else if (depth > 1) { depth--; draw(1); }
      break;
    case A_HOME: if (v->mod >= 0) thumb_capture(); open_save_current(); { int k; for (k = 0; k < depth; k++) if (stack[k].mod < 0 && stack[k].type == V_SPLIT) { split_exit(-1); break; } } depth = 1; stack[0].page = 0; draw(1); break;
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
    case A_STORE_REFRESH: store_refresh(0); v->page = 0; draw(0); break;
    case A_STORE_ALL: { int i, n = 0, need[MAXCAT];
      for (i = 0; i < ncat; i++) { int pv = pkg_ver(cat[i].id); need[i] = !(pv && pv >= cat[i].ver && mod_find(cat[i].id) >= 0) && !pkg_queued(&cat[i]); n += need[i]; }
      instPend += n;
      for (i = 0; i < ncat; i++) if (need[i]) pkg_install_c(&cat[i], 1);
      draw(0); } break;
    case A_PKG: push_view(-1, V_PKG, arg, 0, 0, NULL); draw(1); break;
    case A_PKG_INSTALL: if (arg >= 0 && arg < ncat) { pkg_install(&cat[arg]); draw(0); } break;
    case A_PKG_REMOVE: pkgSel = arg; confirm("Desinstal·la", "Es traurà el paquet del lector. Les teves dades es conserven.", "Desinstal·la", pkg_remove); break;
    case A_UPD_CHECK: upd_check(); draw(0); break;
    case A_UPD_INSTALL: upd_install(); draw(0); break;
    case A_WIFI: NetConnect(NULL); draw(1); break;
    case A_AVISOS: if (v->mod >= 0) thumb_capture();
      if (!(v->mod < 0 && v->type == V_AVISOS)) { push_view(-1, V_AVISOS, 0, 0, 0, NULL); draw(1); } break;
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
    case A_OPEN:
      if (arg >= 0 && arg < nOpen) { int m = mod_find(opened[arg].id); depth = 1; if (m >= 0) open_module(m); else open_drop(arg); draw(1); }
      break;
    case A_MON_STOP:
      if (arg >= 3000) jobs_cancel(arg - 3000);
      else if (arg >= 2000) { int b = arg - 2000; if (b < nbusy) { char id[48]; int k; snprintf(id, sizeof(id), "%s", busyT[b].id); for (k = 0; k < nunl; k++) if (!strcmp(unl[k].id, id)) { unl[k].fn(); break; } h_busy(id, NULL); } }
      else if (arg >= 1000) open_close(arg - 1000);
      draw(0); break;
    case A_OPEN_SPLIT: push_view(-1, V_SPLITPICK, 0, 0, 0, NULL); draw(1); break;
    case A_PICK_TOP: pickTop = arg; if (pickBot == arg) pickBot = -1; draw(0); break;
    case A_PICK_BOT: pickBot = arg; if (pickTop == arg) pickTop = -1; draw(0); break;
    case A_PICK_GO:
      if (pickTop >= 0 && pickBot >= 0 && pickTop != pickBot && pickTop < nmods && pickBot < nmods) {
        split_fill(0, pickTop); split_fill(1, pickBot); splitRatio = 0; paneLast = 0;
        depth = 1; push_view(-1, V_SPLIT, 0, 0, 0, NULL); draw(1);
      }
      break;
    case A_SPLIT_SWAP: { char t[48]; View tv[12]; int n; memcpy(t, splitId[0], 48); memcpy(splitId[0], splitId[1], 48); memcpy(splitId[1], t, 48);
      memcpy(tv, splitSt[0], sizeof(tv)); memcpy(splitSt[0], splitSt[1], sizeof(tv)); memcpy(splitSt[1], tv, sizeof(tv)); n = splitN[0]; splitN[0] = splitN[1]; splitN[1] = n; draw(1); } break;
    case A_SPLIT_SIZE: splitRatio = (splitRatio + 1) % 3; draw(1); break;
    case A_SPLIT_CLOSE: split_exit(-1); draw(1); break;
    case A_OPEN_CLOSE: open_close(arg); draw(0); break;
    case A_OPEN_ALL: while (nOpen) open_close(0); draw(0); break;
    case A_JOB_CANCEL: jobs_cancel(arg); draw(0); break;
    case A_SYNC_TOGGLE: if (arg >= 0 && arg < nbg) { bgt[arg].on = !bgt[arg].on; bg_save(); } draw(0); break;
    case A_SYNC_CHARGE: cfgCharge = !cfgCharge; cfg_save(); draw(0); break;
    case A_SYNC_NOW: bg_now(); draw(0); break;
    case A_QUICK: if (v->mod >= 0) thumb_capture(); if (!(v->mod < 0 && v->type == V_QUICK)) { push_view(-1, V_QUICK, 0, 0, 0, NULL); draw(1); } break;
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
    case A_QGO: { static const int GO[6] = { V_SYSTEM, V_STORE, V_HOME, V_AVISOS, V_OPEN, V_MON };
      depth--; push_view(-1, GO[arg >= 0 && arg < 6 ? arg : 0], 0, 0, 0, NULL); draw(1); } break;
    case A_QDARK: cfgTheme = !cfgTheme; cfg_save(); theme_apply(); draw(2); break;
    case A_QLIGHT_SET: if (light_get() < 0) break; if (arg > 0) SetFrontlightEnabled(1); SetFrontlightState(arg); if (arg == 0) SetFrontlightEnabled(0); draw(0); break;
    case A_HE_TOGGLE: if (arg >= 0 && arg < nwl) { home_toggle(arg); draw(0); } break;
    case A_HE_UP: if (arg >= 0 && arg < nwl) { home_move(arg, -1); draw(0); } break;
    case A_HE_DN: if (arg >= 0 && arg < nwl) { home_move(arg, 1); draw(0); } break;
  }
}

/* Tocs directes per al paquet de primer pla que els vol (escriptori_event): no amb el tauler ràpid, la pantalla dividida
   ni un diàleg obert. */
static int mod_raw_event(int type, int par1, int par2) {
  View *v;
  if (type != EVT_POINTERDOWN && type != EVT_POINTERUP && type != EVT_POINTERMOVE && type != EVT_POINTERDRAG && type != EVT_POINTERLONG &&
      type != EVT_POINTERHOLD && type != EVT_MTSYNC) return 0;
  if (depth < 1 || uiModal || split_on() || quick_open()) return 0;
  v = &stack[depth - 1];
  if (v->mod < 0 || v->mod >= nmods || !mods[v->mod].ev) return 0;
  return mods[v->mod].ev(type, par1, par2);
}
static int handler(int type, int par1, int par2) {
  int i;
  if (mod_raw_event(type, par1, par2)) return 1;
  switch (type) {
    case EVT_INIT:
      SW = ScreenWidth(); SH = ScreenHeight(); U = SW / 27; if (U < 16) U = 16; SB = U * SBK / 4;
      SetPanelType(PANEL_DISABLED);
      SH -= PanelHeight();
      scrSB = SB; scrSH = SH; inPane = -1;
      cfg_load(); theme_apply(); fonts_apply();
      host.api = ESC_API; host.SW = SW; host.SH = SH; host.U = U; host.SB = SB;
      host_fonts(); host.pal = escPal; host.icon_fg = PAL(P_FG); host.icon_bg = PAL(P_CARD); host.update = upd_region;
      host.pages = &pages; host.add_hit = add_hit; host.button = button; host.header = header; host.redraw = draw; host.push = h_push; host.pop = h_pop;
      host.say = say; host.ask = ask; host.confirm = confirm; host.net_get = net_get_t; host.notify = avisos_add; host.status_changed = sb_refresh; host.front = h_front; host.on_unload = h_on_unload;
      host.oy = 0; host.fetch = h_fetch; host.bg_register = bg_register; host.busy = h_busy;
      bg_load(); bg_register("cataleg", "Catàleg de paquets", 24 * 60, bg_catalog);
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
        if (par1 >= hits[i].x && par1 < hits[i].x + hits[i].w && par2 >= hits[i].y && par2 < hits[i].y + hits[i].h) { if (hits[i].pane >= 0) split_act(hits[i].pane, hits[i].act, hits[i].arg); else act(hits[i].act, hits[i].arg); break; }
      break;
    case EVT_KEYPRESS:
      if (par1 == IV_KEY_PREV) { if (split_on()) split_act(paneLast, A_PREV, 0); else act(A_PREV, 0); }
      else if (par1 == IV_KEY_NEXT) { if (split_on()) split_act(paneLast, A_NEXT, 0); else act(A_NEXT, 0); }
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
