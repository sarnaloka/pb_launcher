#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include "inkview.h"
#include "afm.h"
typedef struct { char kind; int x, y, w, h, color, size, bold, flags; char text[700]; char *data; } Op;
static Op ops[40000]; static int nops; static const ifont *cur; static int curColor; static iv_handler H; static char pendingKbd[100];
static int updates; static int dlgBtn = 2;
int ScreenWidth(void) { return 1072; } int ScreenHeight(void) { return 1448; }
void ClearScreen(void) { int i; for (i = 0; i < nops; i++) { free(ops[i].data); ops[i].data = NULL; } nops = 0; }
static Op *add(char k, int x, int y, int w, int h, int color) { Op *o; if (nops >= 40000) { fprintf(stderr, "too many ops\n"); exit(2); } o = &ops[nops++]; memset(o, 0, sizeof(*o)); o->kind = k; o->x = x; o->y = y; o->w = w; o->h = h; o->color = color; return o; }
void DrawLine(int x1, int y1, int x2, int y2, int c) { add('L', x1, y1, x2, y2, c); }
void DrawRect(int x, int y, int w, int h, int c) { add('R', x, y, w, h, c); }
void FillArea(int x, int y, int w, int h, int c) { add('F', x, y, w, h, c); }
ifont *OpenFont(const char *name, int size, int aa) { ifont *f = calloc(1, sizeof(ifont)); (void)aa; f->size = size; f->bold = strstr(name, "old") != NULL; f->serif = strstr(name, "Serif") || strstr(name, "Literata") || strstr(name, "Georgia"); snprintf(f->name, sizeof(f->name), "%s", name); return f; }
void CloseFont(ifont *f) { free(f); }
/* Fonts del simulador: unes quantes de fixes més les de flash/system/fonts. */
char **EnumFonts(void) {
  static char *list[64]; static char store[64][80]; int n = 0, i; DIR *d; struct dirent *e;
  static const char *FIX[] = { "LiberationSans", "LiberationSans-Bold", "LiberationSans-Italic", "LiberationSerif", "LiberationSerif-Bold", "DejaVuSans", "DejaVuSans-Bold", "DejaVuSerif", "DejaVuSerif-Bold" };
  for (i = 0; i < 9; i++) { snprintf(store[n], 80, "%s", FIX[i]); list[n] = store[n]; n++; }
  if ((d = opendir(USERFONTDIR))) { while ((e = readdir(d)) && n < 62) { char *dot = strrchr(e->d_name, '.'); if (!dot || (strcasecmp(dot, ".ttf") && strcasecmp(dot, ".otf"))) continue; snprintf(store[n], 80, "%.*s", (int)(dot - e->d_name), e->d_name); list[n] = store[n]; n++; } closedir(d); }
  list[n] = NULL; return list;
}
static Op *add(char k, int x, int y, int w, int h, int color);
void DrawBitmap(int x, int y, const ibitmap *b) {
  Op *o = NULL; int i, j; static const char *hx = "0123456789abcdef";
  if (b->width * b->height >= 100000)            /* només si no hi ha cap inversió després (l'ordre importa) */
    for (i = nops - 1; i >= 0 && ops[i].kind != 'I'; i--) if (ops[i].kind == 'B' && ops[i].x == x && ops[i].y == y && ops[i].w == b->width && ops[i].h == b->height) { o = &ops[i]; free(o->data); o->data = NULL; break; }
  if (!o) o = add('B', x, y, b->width, b->height, 0);
  o->data = malloc((size_t)b->width * b->height * 2 + 1);
  for (j = 0; j < b->height; j++) for (i = 0; i < b->width; i++) { unsigned char v = b->data[j * b->scanline + i]; o->data[(j * b->width + i) * 2] = hx[v >> 4]; o->data[(j * b->width + i) * 2 + 1] = hx[v & 15]; }
  o->data[(size_t)b->width * b->height * 2] = 0;
}
void DrawPixel(int x, int y, int c) { add('F', x, y, 1, 1, c); }
void DrawCircle(int x0, int y0, int r, int c) { add('R', x0 - r, y0 - r, 2 * r, 2 * r, c); }
void DynamicUpdateA2(int x, int y, int w, int h) { PartialUpdate(x, y, w, h); } int IsInA2Update(void) { return 0; }
void DitherAreaPattern2Level(int dx, int dy, int dw, int dh) { (void)dx; (void)dy; (void)dw; (void)dh; }
static iv_mtinfo simMt[2];
iv_mtinfo *GetTouchInfoI(unsigned int index) { (void)index; return simMt; }
/* Escaquer d'un píxel (al lector s'escriu directament al llenç): render.py el dibuixa. */
void SimChecker(int x, int y, int w, int h) { add('K', x, y, w, h, 0); }
/* El simulador no té píxels: la captura és un gris pla (l'app la mostra com a miniatura). */
ibitmap *BitmapFromScreen(int x, int y, int w, int h) { ibitmap *b = malloc(sizeof(ibitmap) + (size_t)w * h); (void)x; (void)y; b->width = w; b->height = h; b->depth = 8; b->scanline = w; memset(b->data, 200, (size_t)w * h); return b; }
void SetClip(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void InvertArea(int x, int y, int w, int h) { add('I', x, y, w, h, 0); }
void DitherArea(int x, int y, int w, int h, int levels, int method) { (void)x; (void)y; (void)w; (void)h; (void)levels; (void)method; }
/* Imatges: el simulador no descodifica; si el fitxer existeix, torna un degradat de la mida demanada. */
static ibitmap *fake_img(const char *path, int w, int h) {
  FILE *f = fopen(path, "rb"); ibitmap *b; int i, j;
  if (!f) return NULL;
  fclose(f);
  b = malloc(sizeof(ibitmap) + (size_t)w * h); b->width = w; b->height = h; b->depth = 8; b->scanline = w;
  for (j = 0; j < h; j++) for (i = 0; i < w; i++) b->data[j * w + i] = (unsigned char)(90 + 120 * (i + j) / (w + h));
  return b;
}
ibitmap *LoadJPEG(const char *path, int w, int h, int br, int co, int proportional) { (void)br; (void)co; (void)proportional; return fake_img(path, w, h); }
ibitmap *LoadPNGStretch(const char *path, int w, int h, int proportional, int dither) { (void)proportional; (void)dither; return fake_img(path, w, h); }
void SetFont(const ifont *f, int color) { cur = f; curColor = color; }
int StringWidth(const char *s) { const unsigned char *p = (const unsigned char *)s; double w = 0; const unsigned short *W = cur->bold ? W_BOLD : W_REG;
  while (*p) { int c = *p++; if (c >= 0xC0) { while ((*p & 0xC0) == 0x80) p++; c = 'e'; } w += W[c - 32 < 0 ? 0 : c - 32]; } return (int)(w * cur->size / 1000.0 + 0.5); }
char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags) { Op *o;
  if (w <= 0 || h <= 0) { fprintf(stderr, "BAD RECT %d %d %d %d '%s'\n", x, y, w, h, s); }
  o = add('T', x, y, w, h, curColor); o->size = cur->size; o->bold = cur->bold | (cur->serif ? 2 : 0); o->flags = flags; snprintf(o->text, sizeof(o->text), "%s", s); { char *q; for (q = o->text; *q; q++) if (*q == '\n' || *q == '\t') *q = ' '; } return NULL; }
void FullUpdate(void) { updates++; } void SoftUpdate(void) { updates++; }
char *GetDeviceModel(void) { return "PocketBook Verse Pro (sim)"; } char *GetSerialNumber(void) { return "SIM-0000"; } char *GetSoftwareVersion(void) { return "simulador"; }
static int simBatt = 62, simChg, simNet = -1, simSig = 3;
int GetBatteryPower(void) { return simBatt; } int IsCharging(void) { return simChg; } int QueryNetwork(void) { const char *e = getenv("NET"); return simNet >= 0 ? simNet : e ? (int)strtol(e, NULL, 0) : 0x0202; }
void SetPanelType(int t) { (void)t; } int PanelHeight(void) { return 0; } int GetNetSignalQuality(void) { return simSig; }
/* Temporitzadors: diversos alhora, per nom (com a l'InkView). «tick» executa els que tenen «clock» al nom; «poll», la
   resta. Són d'un sol tret: s'esborren abans de cridar-los i el codi els torna a armar. */
#define MAXT 16
static struct { char name[64]; iv_timerproc p; iv_timerprocEx px; void *ctx; } tms[MAXT];
#define TM_USED(i) (tms[i].p || tms[i].px)
static void tm_set2(const char *n, iv_timerproc tp, iv_timerprocEx px, void *ctx) {
  int i, f = -1;
  for (i = 0; i < MAXT; i++) { if (TM_USED(i) && !strcmp(tms[i].name, n ? n : "")) { tms[i].p = tp; tms[i].px = px; tms[i].ctx = ctx; return; } if (!TM_USED(i) && f < 0) f = i; }
  if (f >= 0) { snprintf(tms[f].name, sizeof(tms[f].name), "%s", n ? n : ""); tms[f].p = tp; tms[f].px = px; tms[f].ctx = ctx; }
  else fprintf(stderr, "TOO MANY TIMERS\n");
}
static void tm_set(const char *n, iv_timerproc tp) { tm_set2(n, tp, NULL, NULL); }
static void tm_run(int clock) {
  iv_timerproc run[MAXT]; iv_timerprocEx runx[MAXT]; void *ctx[MAXT]; int i, n = 0;
  for (i = 0; i < MAXT; i++) if (TM_USED(i) && (strstr(tms[i].name, "clock") != NULL) == clock) { run[n] = tms[i].p; runx[n] = tms[i].px; ctx[n] = tms[i].ctx; n++; tms[i].p = NULL; tms[i].px = NULL; }
  for (i = 0; i < n; i++) { if (run[i]) run[i](); else runx[i](ctx[i]); }
}
void SetWeakTimerEx(const char *n, iv_timerprocEx tp, void *context, int ms) { tm_set2(n, NULL, tp, context); if (ms <= 0 || ms > 61000) fprintf(stderr, "BAD TIMER %d\n", ms); }
void SetWeakTimer(const char *n, iv_timerproc tp, int ms) { tm_set(n, tp); if (ms <= 0 || ms > 61000) fprintf(stderr, "BAD TIMER %d\n", ms); }
void SetHardTimer(const char *n, iv_timerproc tp, int ms) { SetWeakTimer(n, tp, ms); }
void ClearTimer(iv_timerproc tp) { int i; for (i = 0; i < MAXT; i++) if (tms[i].p == tp || (tms[i].px && (void *)tms[i].px == (void *)tp)) { tms[i].p = NULL; tms[i].px = NULL; } } static void shot(const char *name);
void PartialUpdate(int x, int y, int w, int h) { static int n; if (getenv("PULOG")) printf("PU %d %d %d %d\n", x, y, w, h); if (getenv("PUSHOT")) { char fn[64]; snprintf(fn, sizeof(fn), "pu%d.txt", ++n); shot(fn); } }
int NetConnect(const char *n) { (void)n; return 0; }
int NetDisconnect(void) { printf("NetDisconnect\n"); return 0; }
static int flState = 30, flOn = 1; int GetFrontlightState(void) { return getenv("NOLIGHT") ? INT_MIN : flState; } void SetFrontlightState(int s) { printf("light %d\n", s); flState = s; } int GetFrontlightEnabled(void) { return flOn; } void SetFrontlightEnabled(int e) { flOn = e; }
void *QuickDownload(const char *url, int *retsize, int timeout) { char fn[600]; const char *m = strstr(url, "/main/"); FILE *f; long n; char *b, *q; (void)timeout;
  if (strstr(url, "geocoding-api")) snprintf(fn, sizeof(fn), "net/geocode.json"); else if (strstr(url, "open-meteo")) snprintf(fn, sizeof(fn), "net/weather.json"); else if (strstr(url, "eurofxref")) snprintf(fn, sizeof(fn), "net/ecb.xml"); else if (m) { snprintf(fn, sizeof(fn), "net/repo/%s", m + 6); if ((q = strchr(fn, '?'))) *q = 0; } else snprintf(fn, sizeof(fn), "net/feed.xml");
  printf("download %s\n", fn); f = fopen(fn, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET); b = malloc((size_t)n + 1); n = (long)fread(b, 1, (size_t)n, f); fclose(f); *retsize = (int)n; return b; }
int TextRectHeight(int width, const char *s, int flags) { (void)flags; return (StringWidth(s) / (width > 0 ? width : 1) + 1) * cur->size * 13 / 10; }
void ShowHourglass(void) {} void HideHourglass(void) {}
void CloseApp(void) { printf("CloseApp\n"); H(EVT_EXIT, 0, 0); exit(0); }
void OpenKeyboard(const char *title, char *buffer, int maxlen, int flags, iv_keyboardhandler hp) { (void)flags; printf("keyboard: %s -> '%s'\n", title, pendingKbd); snprintf(buffer, (size_t)maxlen, "%s", pendingKbd); hp(buffer); }
void Message(int icon, const char *title, const char *text, int timeout) { (void)icon; (void)timeout; printf("MESSAGE %s: %s\n", title, text); }
void Dialog(int icon, const char *title, const char *text, const char *b1, const char *b2, iv_dialoghandler hp) { (void)icon; printf("DIALOG %s: %s [%s|%s]\n", title, text, b1, b2); hp(dlgBtn); }
int OpenBook(const char *path, const char *p, int f) { (void)p; (void)f; printf("OpenBook %s\n", path); return 1; }
static void shot(const char *name) { FILE *f = fopen(name, "w"); int i; for (i = 0; i < nops; i++) { Op *o = &ops[i]; fprintf(f, "%c\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", o->kind, o->x, o->y, o->w, o->h, o->color, o->size, o->bold, o->flags, o->data ? o->data : o->text); } fclose(f); }
void InkViewMain(iv_handler h) { char ln[300]; H = h; h(EVT_INIT, 0, 0); h(EVT_SHOW, 0, 0);
  while (fgets(ln, sizeof(ln), stdin)) { char *nl = strchr(ln, '\n'); int i, x, y; if (nl) *nl = 0;
    if (!strncmp(ln, "tapp ", 5)) {          /* com «tap», però passa pàgina (fins a 5 cops) si no ho troba */
      int k, found = 0;
      for (k = 0; k < 6 && !found; k++) {
        for (i = nops - 1; i >= 0 && !found; i--) if (ops[i].kind == 'T' && !strcmp(ops[i].text, ln + 5)) { found = 1; h(EVT_POINTERUP, ops[i].x + ops[i].w / 2, ops[i].y + ops[i].h / 2); }
        if (!found && k < 5) h(EVT_KEYPRESS, 0x19, 0);
      }
      if (!found) printf("NOT FOUND: %s\n", ln + 5);
    }
    else if (!strncmp(ln, "tap ", 4)) { int found = 0; for (i = nops - 1; i >= 0; i--) if (ops[i].kind == 'T' && !strcmp(ops[i].text, ln + 4)) { found = 1; h(EVT_POINTERUP, ops[i].x + ops[i].w / 2, ops[i].y + ops[i].h / 2); break; }
      if (!found) for (i = nops - 1; i >= 0; i--) if (ops[i].kind == 'T' && strstr(ops[i].text, ln + 4)) { found = 1; h(EVT_POINTERUP, ops[i].x + ops[i].w / 2, ops[i].y + ops[i].h / 2); break; }
      if (!found) { printf("NOT FOUND: %s\n", ln + 4); } }
    else if (sscanf(ln, "xy %d %d", &x, &y) == 2) h(EVT_POINTERUP, x, y);
    else if (sscanf(ln, "mt %d %d", &x, &y) == 2) { simMt[0].active = 1; simMt[0].x = x; simMt[0].y = y; h(EVT_POINTERDOWN, x, y); h(EVT_MTSYNC, 0, 1); }
    else if (!strcmp(ln, "mtup")) { simMt[0].active = 0; h(EVT_MTSYNC, 0, 0); h(EVT_POINTERUP, simMt[0].x, simMt[0].y); }
    else if (sscanf(ln, "polls %d", &x) == 1) { for (i = 0; i < x; i++) tm_run(0); }
    else if (sscanf(ln, "key %d", &x) == 1) h(EVT_KEYPRESS, x, 0);
    else if (!strncmp(ln, "kbd ", 4)) snprintf(pendingKbd, sizeof(pendingKbd), "%s", ln + 4);
    else if (sscanf(ln, "dlg %d", &x) == 1) dlgBtn = x;
    else if (!strncmp(ln, "shot ", 5)) shot(ln + 5);
    else if (!strcmp(ln, "tick")) tm_run(1);
    else if (!strcmp(ln, "poll")) tm_run(0);
    else if (sscanf(ln, "sleep %d", &x) == 1) usleep((useconds_t)x * 1000);
    else if (sscanf(ln, "batt %d", &x) == 1) simBatt = x;
    else if (sscanf(ln, "charge %d", &x) == 1) simChg = x;
    else if (!strncmp(ln, "net ", 4)) simNet = (int)strtol(ln + 4, NULL, 0);
    else if (sscanf(ln, "sig %d", &x) == 1) simSig = x;
    else if (sscanf(ln, "evt %d", &x) == 1) h(x, 0, 0);
    else if (!strncmp(ln, "echo ", 5)) printf("%s\n", ln + 5);
  }
  printf("updates %d\n", updates); h(EVT_EXIT, 0, 0); }
