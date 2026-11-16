/* Escriptori — interfície entre el sistema base i els paquets (mòduls). */
#ifndef ESC_MOD_H
#define ESC_MOD_H
#include "inkview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#define ESC_API 3                  /* 3 = Host.notify i Host.status_changed. Els paquets que no els necessiten poden declarar ESC_API_MIN */
#define ESC_API_MIN 2
#define MOD_TICK 1                 /* el paquet vol redibuixar-se cada minut */
#define NOTELEN 400
#define MAXL 300
#define CFG FLASHDIR "/system/config/"

typedef struct { int type, a, b, page, sel; char term[64]; int mod; } View;
typedef struct Host {
  int api, SW, SH, U, SB;
  ifont *fTitle, *fBold, *fText, *fSmall, *fBig, *fHuge;
  int *pages;
  void (*add_hit)(int x, int y, int w, int h, int act, int arg);
  void (*button)(int x, int y, int w, int h, const char *label, int inv, int act, int arg);
  void (*header)(const char *title, const char *sub);
  void (*redraw)(int full);
  void (*push)(int type, int a, int b, int sel, const char *term);
  void (*pop)(void);
  void (*say)(const char *msg);
  void (*ask)(const char *title, const char *initial, void (*fn)(const char *));
  void (*confirm)(const char *title, const char *text, const char *yes, void (*fn)(void));
  char *(*net_get)(const char *url, int *len, int timeout);
  /* només si host->api >= 3 */
  void (*notify)(const char *src, const char *title, const char *text);   /* afegeix un avís (src = nom del paquet) */
  void (*status_changed)(void);                                          /* l'estat d'una ranura ha canviat: repinta només la barra */
} Host;
/* Ranura de la barra d'estat: un indicador petit. width() retorna l'amplada en píxels (0 = ara no es mostra);
   draw() el dibuixa dins el rectangle x,y,w,h amb el color fg. La barra és NEGRA: fg és blanc i el fons, negre (rcard(..., fg, BLACK)).
   Per quedar alineat amb la resta, centra'l verticalment i fes-lo d'una alçada d'uns 2/5 de h. S'avalua a cada refresc: ha de ser barat. */
typedef struct StatusItem { const char *id; int (*width)(int h); void (*draw)(int x, int y, int w, int h, int fg); } StatusItem;
/* Widget de l'escriptori. cw = columnes que ocupa (2 o 4 de 4); ch = alçada en unitats de 3U. El sistema dibuixa la
   targeta; draw() omple l'interior x,y,w,h. En tocar-lo s'obre el paquet. */
typedef struct Widget { const char *id, *name; int cw, ch; void (*draw)(int x, int y, int w, int h); } Widget;
typedef struct Module {
  int api; const char *id; const char *name; int flags;
  void (*fn_icon)(int x, int y, int s);
  void (*fn_draw)(View *v);
  void (*fn_action)(View *v, int act, int arg);     /* accions amb codi >= 100 */
  int  (*fn_back)(View *v);                         /* 1 = el paquet ja ha gestionat «enrere» */
  void (*fn_open)(View *v);                         /* en obrir-lo des de l'escriptori */
  int nstatus; const StatusItem *status;            /* ranures de la barra d'estat (opcional) */
  int nwidgets; const Widget *widgets;              /* widgets de l'escriptori (opcional) */
} Module;

#define UNUSED __attribute__((unused))
/* ───────────── sistema de disseny ─────────────
   Formes simples sobre fons blanc: targetes de cantonades arrodonides, icones de línia pròpies. */
static UNUSED void rfill(int x, int y, int w, int h, int r, int col) {
  int k;
  if (w <= 0 || h <= 0) return;
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  if (r < 2) { FillArea(x, y, w, h, col); return; }
  if (h - 2 * r > 0) FillArea(x, y + r, w, h - 2 * r, col);
  for (k = 0; k < r; k++) {
    double dy = r - k - 0.5; int inset = r - (int)(sqrt((double)r * r - dy * dy) + 0.5);
    FillArea(x + inset, y + k, w - 2 * inset, 1, col);
    FillArea(x + inset, y + h - 1 - k, w - 2 * inset, 1, col);
  }
}
static UNUSED int ui_r(int w, int h) { int m = w < h ? w : h, r = m / 4; return r < 3 ? 3 : r > 40 ? 40 : r; }
/* t = gruix de la vora (0 = sense vora), fg = color de la vora, bg = color de l'interior */
static UNUSED void rcard(int x, int y, int w, int h, int r, int t, int fg, int bg) {
  rfill(x, y, w, h, r, fg);
  if (t > 0) rfill(x + t, y + t, w - 2 * t, h - 2 * t, r > t ? r - t : 0, bg);
}
enum { UI_LINE, UI_SOFT, UI_SOLID };
static UNUSED void ui_card(int x, int y, int w, int h, int style) {
  int r = ui_r(w, h);
  if (style == UI_SOLID) rcard(x, y, w, h, r, 0, BLACK, BLACK);
  else rcard(x, y, w, h, r, 3, style == UI_SOFT ? LGRAY : BLACK, WHITE);
}
static UNUSED void box(int x, int y, int w, int h) { ui_card(x, y, w, h, UI_LINE); }
static UNUSED void disc(int cx, int cy, int r, int col) {
  int dy;
  for (dy = -r; dy <= r; dy++) { int h = (int)(sqrt((double)(r * r - dy * dy)) + 0.5); if (h > 0) FillArea(cx - h, cy + dy, 2 * h, 1, col); }
}
static UNUSED void thick(int x1, int y1, int x2, int y2, int t, int col) {
  int dx = x2 - x1, dy = y2 - y1, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy), i;
  if (n < 1) n = 1;
  for (i = 0; i <= n; i++) FillArea(x1 + dx * i / n - t / 2, y1 + dy * i / n - t / 2, t, t, col);
}

/* Icones de les aplicacions: totes de línia amb el mateix gruix. ic_t(s) = gruix per a una icona de mida s;
   ol() = rectangle arrodonit de contorn; ring() = anella. Es dibuixen en negre sobre fons blanc. */
static UNUSED int ic_t(int s) { return s / 14 > 3 ? s / 14 : 3; }
static UNUSED void ol(int x, int y, int w, int h, int r, int t) { rcard(x, y, w, h, r, t, BLACK, WHITE); }
static UNUSED void ring(int cx, int cy, int r, int t) { disc(cx, cy, r, BLACK); disc(cx, cy, r - t, WHITE); }
static UNUSED void rays(int cx, int cy, int r0, int r1, int t) {
  int k;
  for (k = 0; k < 8; k++) { double a = k * 3.14159265 / 4; thick(cx + (int)(sin(a) * r0), cy - (int)(cos(a) * r0), cx + (int)(sin(a) * r1), cy - (int)(cos(a) * r1), t, BLACK); }
}
/* Icones de línia: cx,cy = centre; r = meitat de la mida; col = color. */
static UNUSED void ic_arc(int cx, int cy, int R, double a0, double a1, int t, int col) {   /* angles en graus des de la vertical */
  double len = (a1 - a0) * 3.14159265 / 180.0 * R; int n = (int)(len / (t > 1 ? t * 0.7 : 1)) + 1, i;
  for (i = 0; i <= n; i++) {
    double a = (a0 + (a1 - a0) * i / n) * 3.14159265 / 180.0;
    FillArea(cx + (int)(R * sin(a) + (sin(a) < 0 ? -0.5 : 0.5)) - t / 2, cy - (int)(R * cos(a) + (cos(a) < 0 ? -0.5 : 0.5)) - t / 2, t, t, col);
  }
}
/* Barres de senyal (4, creixents). level 0..4 = barres enceses (color onc), la resta en offc. cross = color d'una petita «x»
   a dalt a l'esquerra de les barres (-1 = sense x): senyal d'apagat o desconnectat. cx,cy = centre; h = alçada. */
static UNUSED int ic_signal_w(int h) { int bw = h / 5 > 2 ? h / 5 : 2, gap = bw / 2 > 1 ? bw / 2 : 1; return 4 * bw + 3 * gap; }
static UNUSED void ic_signal(int cx, int cy, int h, int level, int onc, int offc, int cross) {
  int bw = h / 5 > 2 ? h / 5 : 2, gap = bw / 2 > 1 ? bw / 2 : 1, x0 = cx - ic_signal_w(h) / 2, y1 = cy + h / 2, i;
  for (i = 0; i < 4; i++) { int bh = h * (i + 1) / 4; rfill(x0 + i * (bw + gap), y1 - bh, bw, bh, bw / 3, i < level ? onc : offc); }
  if (cross >= 0) {
    int xs = h * 4 / 10, t = bw * 3 / 5 > 2 ? bw * 3 / 5 : 2, top = y1 - h;
    thick(x0, top, x0 + xs, top + xs, t, cross); thick(x0, top + xs, x0 + xs, top, t, cross);
  }
}
/* Campana (avisos): silueta plena amb cúpula, vora ampla, boleta a dalt i batall. bg = fons (per al forat del cos). */
static UNUSED void ic_bell(int cx, int cy, int h, int col, int bg) {
  int bw = h * 62 / 100, top = cy - h / 2, kn = h / 9 > 2 ? h / 9 : 2, bt = top + kn + 1, bh = h * 56 / 100, fw = h * 94 / 100, fh = h * 17 / 100 > 2 ? h * 17 / 100 : 2;
  rfill(cx - kn, top, 2 * kn, kn + 2, kn, col);
  rfill(cx - bw / 2, bt, bw, bh, bw / 2, col);
  rfill(cx - fw / 2, bt + bh - fh / 2, fw, fh, fh / 2, col);
  rfill(cx - h / 9, bt + bh + fh / 2 + 1, 2 * (h / 9), h / 8 > 2 ? h / 8 : 2, h / 9, col);
  (void)bg;
}
static UNUSED void ic_sun(int cx, int cy, int r, int col) {
  int k, t = r / 8 > 2 ? r / 8 : 2;
  disc(cx, cy, r * 4 / 10, col);
  for (k = 0; k < 8; k++) {
    double a = k * 3.14159265 / 4;
    thick(cx + (int)(sin(a) * r * 0.65), cy - (int)(cos(a) * r * 0.65), cx + (int)(sin(a) * r), cy - (int)(cos(a) * r), t, col);
  }
}
static UNUSED void ic_refresh(int cx, int cy, int r, int col) {
  int t = r / 6 > 2 ? r / 6 : 2, R = r * 8 / 10, hx, hy, k;
  ic_arc(cx, cy, R, 20, 300, t, col);
  hx = cx + (int)(R * sin(20 * 3.14159265 / 180)); hy = cy - (int)(R * cos(20 * 3.14159265 / 180));
  for (k = 0; k < r / 2; k++) FillArea(hx - k, hy - r / 2 + k + r / 8, 2 * k + 1, 1, col);
}
static UNUSED void ic_check(int cx, int cy, int r, int col) {
  int t = r / 5 > 2 ? r / 5 : 3;
  thick(cx - r * 7 / 10, cy, cx - r / 5, cy + r / 2, t, col); thick(cx - r / 5, cy + r / 2, cx + r * 7 / 10, cy - r / 2, t, col);
}
static UNUSED void ic_chevron(int cx, int cy, int r, int col) {
  int t = r / 4 > 2 ? r / 4 : 3;
  thick(cx - r / 3, cy - r / 2, cx + r / 3, cy, t, col); thick(cx + r / 3, cy, cx - r / 3, cy + r / 2, t, col);
}
static UNUSED void ic_plus(int cx, int cy, int r, int col) { int t = r / 4 > 2 ? r / 4 : 3; FillArea(cx - r / 2, cy - t / 2, r, t, col); FillArea(cx - t / 2, cy - r / 2, t, r, col); }
static UNUSED void ic_minus(int cx, int cy, int r, int col) { int t = r / 4 > 2 ? r / 4 : 3; FillArea(cx - r / 2, cy - t / 2, r, t, col); }
static UNUSED void ic_grid(int cx, int cy, int r, int col) {
  int g = r / 5 > 2 ? r / 5 : 2, c = (r * 2 - g) / 2, i;
  for (i = 0; i < 4; i++) rfill(cx - r + (i % 2) * (c + g), cy - r + (i / 2) * (c + g), c, c, c / 4, col);
}
/* Cercle de càrrega de 12 raigs que s'esvaeixen: els 3 primers en c0, els 3 següents en c1 i la resta en c2.
   El cap és el raig de dalt i la resta segueixen en sentit antihorari. cx,cy = centre; r = radi. */
static UNUSED void ic_spinner(int cx, int cy, int r, int c0, int c1, int c2) {
  int k, t = r / 5 > 2 ? r / 5 : 2;
  for (k = 0; k < 12; k++) {
    double a = -k * 3.14159265 / 6;
    thick(cx + (int)(sin(a) * r * 0.5), cy - (int)(cos(a) * r * 0.5), cx + (int)(sin(a) * r), cy - (int)(cos(a) * r), t, k < 3 ? c0 : k < 6 ? c1 : c2);
  }
}
/* fletxa vertical; down = 1 apunta avall. y = dalt, h = alçada. */
static UNUSED void ic_arrow(int cx, int y, int h, int down, int col) {
  int hh = h * 2 / 5, t = h / 8 > 2 ? h / 8 : 2, k;
  FillArea(cx - t / 2, y, t, h, col);
  for (k = 0; k < hh; k++) {
    int half = down ? hh - 1 - k : k, ry = down ? y + h - hh + k : y + k;
    FillArea(cx - half, ry, 2 * half + 1, 1, col);
  }
}

#ifdef ESC_MODULE
static const Host *H;
#define SW (H->SW)
#define SH (H->SH)
#define U (H->U)
#define SB (H->SB)
#define fTitle (H->fTitle)
#define fBold (H->fBold)
#define fText (H->fText)
#define fSmall (H->fSmall)
#define fBig (H->fBig)
#define fHuge (H->fHuge)
#define pages (*H->pages)
#define add_hit H->add_hit
#define button H->button
#define header H->header
#define draw H->redraw
#define push H->push
#define POP() H->pop()
#define say H->say
#define ask H->ask
#define confirm H->confirm
#define TOP (SB + 3 * U + U / 2)
#define NAVH (2 * U + U / 4)
#define BOTTOM (SH - NAVH - U / 2)
static UNUSED char *net_get(const char *url, int *len) { return H->net_get(url, len, 25); }
static UNUSED const char *WDAY[7] = { "diumenge", "dilluns", "dimarts", "dimecres", "dijous", "divendres", "dissabte" };
static UNUSED const char *MONTH[12] = { "de gener", "de febrer", "de març", "d'abril", "de maig", "de juny", "de juliol", "d'agost", "de setembre", "d'octubre", "de novembre", "de desembre" };
static UNUSED const char *MONTHN[12] = { "Gener", "Febrer", "Març", "Abril", "Maig", "Juny", "Juliol", "Agost", "Setembre", "Octubre", "Novembre", "Desembre" };
static UNUSED void fmt_date(char *out, int sz, time_t t) { struct tm *tm = localtime(&t); snprintf(out, (size_t)sz, "%02d/%02d/%d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900); }
typedef struct { char **it; int n, loaded; const char *path; } Store;
static UNUSED void st_load(Store *s) {
  FILE *f; char ln[4096];
  if (s->loaded) return;
  s->loaded = 1; s->n = 0;
  if (!s->it) s->it = (char **)calloc(MAXL, sizeof(char *));
  if (!s->it) return;
  f = fopen(s->path, "r");
  if (!f) return;
  while (s->n < MAXL && fgets(ln, sizeof(ln), f)) {
    char *nl = strpbrk(ln, "\r\n");
    if (nl) *nl = 0;
    if (ln[0] && (s->it[s->n] = strdup(ln))) s->n++;
  }
  fclose(f);
}
static UNUSED void st_save(Store *s) {
  FILE *f = fopen(s->path, "w"); int i;
  if (!f) return;
  for (i = 0; i < s->n; i++) fprintf(f, "%s\n", s->it[i]);
  fclose(f);
}
static UNUSED void st_insert(Store *s, int pos, const char *t) {
  char *c;
  if (!s->it || s->n >= MAXL || !(c = strdup(t))) return;
  if (pos < 0) pos = 0;
  if (pos > s->n) pos = s->n;
  memmove(&s->it[pos + 1], &s->it[pos], sizeof(char *) * (size_t)(s->n - pos));
  s->it[pos] = c; s->n++;
}
static UNUSED void st_del(Store *s, int i) {
  if (i < 0 || i >= s->n) return;
  free(s->it[i]);
  memmove(&s->it[i], &s->it[i + 1], sizeof(char *) * (size_t)(s->n - i - 1));
  s->n--;
}
static UNUSED void st_clear(Store *s) { while (s->n > 0) st_del(s, s->n - 1); }
static UNUSED void fmt_num(char *out, int sz, double v) {
  char *p;
  double a = v < 0 ? -v : v;
  if (a >= 100000 && a < 1e15) snprintf(out, (size_t)sz, (a - floor(a) < 0.005) ? "%.0f" : "%.2f", v);
  else snprintf(out, (size_t)sz, "%.6g", v);
  for (p = out; *p; p++) if (*p == '.') *p = ',';
}
static UNUSED double parse_num(const char *s) { char t[64]; int i; for (i = 0; s[i] && i < 63; i++) t[i] = s[i] == ',' ? '.' : s[i]; t[i] = 0; return atof(t); }
static UNUSED int days_in_month(int y, int m) {
  static const int MD[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return (m == 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) ? 29 : MD[m];
}
static UNUSED int weekday_of(int y, int m, int d) { struct tm tm; memset(&tm, 0, sizeof(tm)); tm.tm_year = y - 1900; tm.tm_mon = m; tm.tm_mday = d; tm.tm_hour = 12; tm.tm_isdst = -1; mktime(&tm); return tm.tm_wday; }
static UNUSED char *read_file(const char *path) {
  FILE *f = fopen(path, "rb"); long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 8 * 1024 * 1024) { fclose(f); return NULL; }
  b = (char *)malloc((size_t)n + 1);
  if (b) { n = (long)fread(b, 1, (size_t)n, f); b[n] = 0; }
  fclose(f);
  return b;
}

#include "ui.h"
#endif
#endif
