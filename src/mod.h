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

#define ESC_API 1
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
} Host;
typedef struct Module {
  int api; const char *id; const char *name; int flags;
  void (*fn_icon)(int x, int y, int s);
  void (*fn_draw)(View *v);
  void (*fn_action)(View *v, int act, int arg);     /* accions amb codi >= 100 */
  int  (*fn_back)(View *v);                         /* 1 = el paquet ja ha gestionat «enrere» */
  void (*fn_open)(View *v);                         /* en obrir-lo des de l'escriptori */
} Module;

#define UNUSED __attribute__((unused))
static UNUSED void box(int x, int y, int w, int h) { DrawRect(x, y, w, h, BLACK); DrawRect(x + 1, y + 1, w - 2, h - 2, BLACK); }
static UNUSED void disc(int cx, int cy, int r, int col) {
  int dy;
  for (dy = -r; dy <= r; dy++) { int h = (int)(sqrt((double)(r * r - dy * dy)) + 0.5); if (h > 0) FillArea(cx - h, cy + dy, 2 * h, 1, col); }
}
static UNUSED void thick(int x1, int y1, int x2, int y2, int t, int col) {
  int dx = x2 - x1, dy = y2 - y1, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy), i;
  if (n < 1) n = 1;
  for (i = 0; i <= n; i++) FillArea(x1 + dx * i / n - t / 2, y1 + dy * i / n - t / 2, t, t, col);
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

#endif
#endif
