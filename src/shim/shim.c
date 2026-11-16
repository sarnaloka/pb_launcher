#include "inkview.h"
#include "afm.h"
typedef struct { char kind; int x, y, w, h, color, size, bold, flags; char text[700]; } Op;
static Op ops[4000]; static int nops; static const ifont *cur; static int curColor; static iv_handler H; static char pendingKbd[100];
static int updates; static int dlgBtn = 2;
int ScreenWidth(void) { return 1072; } int ScreenHeight(void) { return 1448; }
void ClearScreen(void) { nops = 0; }
static Op *add(char k, int x, int y, int w, int h, int color) { Op *o; if (nops >= 4000) { fprintf(stderr, "too many ops\n"); exit(2); } o = &ops[nops++]; memset(o, 0, sizeof(*o)); o->kind = k; o->x = x; o->y = y; o->w = w; o->h = h; o->color = color; return o; }
void DrawLine(int x1, int y1, int x2, int y2, int c) { add('L', x1, y1, x2, y2, c); }
void DrawRect(int x, int y, int w, int h, int c) { add('R', x, y, w, h, c); }
void FillArea(int x, int y, int w, int h, int c) { add('F', x, y, w, h, c); }
ifont *OpenFont(const char *name, int size, int aa) { ifont *f = malloc(sizeof(ifont)); (void)aa; f->size = size; f->bold = strstr(name, "old") != NULL; return f; }
void SetFont(const ifont *f, int color) { cur = f; curColor = color; }
int StringWidth(const char *s) { const unsigned char *p = (const unsigned char *)s; double w = 0; const unsigned short *W = cur->bold ? W_BOLD : W_REG;
  while (*p) { int c = *p++; if (c >= 0xC0) { while ((*p & 0xC0) == 0x80) p++; c = 'e'; } w += W[c - 32 < 0 ? 0 : c - 32]; } return (int)(w * cur->size / 1000.0 + 0.5); }
char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags) { Op *o;
  if (w <= 0 || h <= 0) { fprintf(stderr, "BAD RECT %d %d %d %d '%s'\n", x, y, w, h, s); }
  o = add('T', x, y, w, h, curColor); o->size = cur->size; o->bold = cur->bold; o->flags = flags; snprintf(o->text, sizeof(o->text), "%s", s); { char *q; for (q = o->text; *q; q++) if (*q == '\n' || *q == '\t') *q = ' '; } return NULL; }
void FullUpdate(void) { updates++; } void SoftUpdate(void) { updates++; }
int GetBatteryPower(void) { return 62; } int IsCharging(void) { return 0; } int QueryNetwork(void) { const char *e = getenv("NET"); return e ? (int)strtol(e, NULL, 0) : 0x0202; }
void SetPanelType(int t) { (void)t; } int PanelHeight(void) { return 0; } int GetNetSignalQuality(void) { return 3; }
void SetWeakTimer(const char *n, iv_timerproc tp, int ms) { (void)n; (void)tp; if (ms <= 0 || ms > 61000) fprintf(stderr, "BAD TIMER %d\n", ms); } void ClearTimer(iv_timerproc tp) { (void)tp; } void PartialUpdate(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
int NetConnect(const char *n) { (void)n; return 0; }
void *QuickDownload(const char *url, int *retsize, int timeout) { char fn[600]; const char *m = strstr(url, "/main/"); FILE *f; long n; char *b, *q; (void)timeout;
  if (strstr(url, "open-meteo")) snprintf(fn, sizeof(fn), "net/weather.json"); else if (m) { snprintf(fn, sizeof(fn), "net/repo/%s", m + 6); if ((q = strchr(fn, '?'))) *q = 0; } else snprintf(fn, sizeof(fn), "net/feed.xml");
  printf("download %s\n", fn); f = fopen(fn, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET); b = malloc((size_t)n + 1); n = (long)fread(b, 1, (size_t)n, f); fclose(f); *retsize = (int)n; return b; }
int TextRectHeight(int width, const char *s, int flags) { (void)flags; return (StringWidth(s) / (width > 0 ? width : 1) + 1) * cur->size * 13 / 10; }
void ShowHourglass(void) {} void HideHourglass(void) {}
void CloseApp(void) { printf("CloseApp\n"); H(EVT_EXIT, 0, 0); exit(0); }
void OpenKeyboard(const char *title, char *buffer, int maxlen, int flags, iv_keyboardhandler hp) { (void)flags; printf("keyboard: %s -> '%s'\n", title, pendingKbd); snprintf(buffer, (size_t)maxlen, "%s", pendingKbd); hp(buffer); }
void Message(int icon, const char *title, const char *text, int timeout) { (void)icon; (void)timeout; printf("MESSAGE %s: %s\n", title, text); }
void Dialog(int icon, const char *title, const char *text, const char *b1, const char *b2, iv_dialoghandler hp) { (void)icon; printf("DIALOG %s: %s [%s|%s]\n", title, text, b1, b2); hp(dlgBtn); }
int OpenBook(const char *path, const char *p, int f) { (void)p; (void)f; printf("OpenBook %s\n", path); return 1; }
static void shot(const char *name) { FILE *f = fopen(name, "w"); int i; for (i = 0; i < nops; i++) { Op *o = &ops[i]; fprintf(f, "%c\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", o->kind, o->x, o->y, o->w, o->h, o->color, o->size, o->bold, o->flags, o->text); } fclose(f); }
void InkViewMain(iv_handler h) { char ln[300]; H = h; h(EVT_INIT, 0, 0); h(EVT_SHOW, 0, 0);
  while (fgets(ln, sizeof(ln), stdin)) { char *nl = strchr(ln, '\n'); int i, x, y; if (nl) *nl = 0;
    if (!strncmp(ln, "tap ", 4)) { int found = 0; for (i = nops - 1; i >= 0; i--) if (ops[i].kind == 'T' && !strcmp(ops[i].text, ln + 4)) { found = 1; h(EVT_POINTERUP, ops[i].x + ops[i].w / 2, ops[i].y + ops[i].h / 2); break; }
      if (!found) for (i = nops - 1; i >= 0; i--) if (ops[i].kind == 'T' && strstr(ops[i].text, ln + 4)) { found = 1; h(EVT_POINTERUP, ops[i].x + ops[i].w / 2, ops[i].y + ops[i].h / 2); break; }
      if (!found) { printf("NOT FOUND: %s\n", ln + 4); } }
    else if (sscanf(ln, "xy %d %d", &x, &y) == 2) h(EVT_POINTERUP, x, y);
    else if (sscanf(ln, "key %d", &x) == 1) h(EVT_KEYPRESS, x, 0);
    else if (!strncmp(ln, "kbd ", 4)) snprintf(pendingKbd, sizeof(pendingKbd), "%s", ln + 4);
    else if (sscanf(ln, "dlg %d", &x) == 1) dlgBtn = x;
    else if (!strncmp(ln, "shot ", 5)) shot(ln + 5);
  }
  printf("updates %d\n", updates); h(EVT_EXIT, 0, 0); }
