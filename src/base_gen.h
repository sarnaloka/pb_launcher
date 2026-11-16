/* Peces de dibuix del sistema base (extretes del codi original). */
/* ───────────── dibuix ───────────── */
static void add_hit(int x, int y, int w, int h, int act, int arg) {
  if (nhits < MAXHITS) { hits[nhits].x = x; hits[nhits].y = y; hits[nhits].w = w; hits[nhits].h = h; hits[nhits].act = act; hits[nhits].arg = arg; nhits++; }
}
static void button(int x, int y, int w, int h, const char *label, int inv, int act, int arg) {
  if (inv) FillArea(x, y, w, h, BLACK); else box(x, y, w, h);
  SetFont(fBold, inv ? WHITE : BLACK);
  DrawTextRect(x + 4, y, w - 8, h, label, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
  add_hit(x, y, w, h, act, arg);
}
static void icon_back(int cx, int cy, int r, int col) {
  int dy;
  for (dy = -r; dy <= r; dy++) { int len = r - (dy < 0 ? -dy : dy); if (len > 0) FillArea(cx - r / 4 - len, cy + dy, len, 1, col); }
  FillArea(cx - r / 4 - 1, cy - r / 3, r, 2 * (r / 3) + 1, col);
}
static void icon_home(int cx, int cy, int r, int col) {
  int dy;
  for (dy = 0; dy <= r; dy++) FillArea(cx - dy, cy - r + dy, 2 * dy + 1, 1, col);
  FillArea(cx - r * 2 / 3, cy, 2 * (r * 2 / 3) + 1, r * 3 / 4, col);
  FillArea(cx - r / 6, cy + r / 4, 2 * (r / 6) + 1, r / 2, BLACK);
}
static void icon_exit(int cx, int cy, int r, int col) {
  int i, t = r / 5 > 1 ? r / 5 : 2;
  for (i = -r * 3 / 4; i <= r * 3 / 4; i++) { FillArea(cx + i - t / 2, cy + i - t / 2, t + 1, t + 1, col); FillArea(cx + i - t / 2, cy - i - t / 2, t + 1, t + 1, col); }
}
/* Activitat de xarxa: bits que la barra d'estat mostra com a fletxes (baixada / pujada) al costat del Wi-Fi. */
enum { NETA_DOWN = 1, NETA_UP = 2 };
static int netAct;
/* Fletxa vertical de formes simples: eix + punta triangular. cx = centre, y = dalt, h = alçada. */
static void icon_arrow(int cx, int y, int h, int down, int col) {
  int hh = h * 2 / 5, t = h / 8 > 2 ? h / 8 : 2, k;
  FillArea(cx - t / 2, y, t, h, col);
  for (k = 0; k < hh; k++) {
    int half = down ? hh - 1 - k : k, ry = down ? y + h - hh + k : y + k;
    FillArea(cx - half, ry, 2 * half + 1, 1, col);
  }
}
/* Barra d'estat a l'estil d'Android 2.3: fons negre; a l'esquerra, navegació (enrere, inici, surt);
   a la dreta, cobertura (o OFF / ECO), percentatge i icona de bateria, i hora. */
static void statusbar(int withHits) {
  char t[16]; time_t now = time(NULL); struct tm *tm = localtime(&now);
  int x = SW - U / 2, w, i, bars = 0, pct = GetBatteryPower(), ih = SB / 2, iy = (SB - ih) / 2, net = QueryNetwork();
  int bw = U * 6 / 10, sw = U / 4, gap = U / 8 > 2 ? U / 8 : 2, nb = SB * 5 / 4, r = SB / 4;
  FillArea(0, 0, SW, SB, BLACK);
  /* navegació */
  icon_back(nb / 2, SB / 2, r, depth > 1 ? WHITE : DGRAY);
  icon_home(nb + nb / 2, SB / 2, r, depth > 1 ? WHITE : DGRAY);
  icon_exit(2 * nb + nb / 2, SB / 2, r, WHITE);
  FillArea(nb, SB / 4, 1, SB / 2, DGRAY); FillArea(2 * nb, SB / 4, 1, SB / 2, DGRAY); FillArea(3 * nb, SB / 4, 1, SB / 2, DGRAY);
  if (withHits) {
    if (depth > 1) { add_hit(0, 0, nb, SB, A_BACK, 0); add_hit(nb, 0, nb, SB, A_HOME, 0); }
    add_hit(2 * nb, 0, nb, SB, A_EXIT, 0);
  }
  /* hora */
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  SetFont(fTitle, WHITE); w = StringWidth(t);
  DrawTextRect(x - w, 0, w, SB, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  x -= w + U / 2;
  /* bateria horitzontal amb born a la dreta; llamp quan carrega; percentatge al costat */
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  {
    int bh = ih * 7 / 10, by = (SB - bh) / 2, nw = bw / 6 > 3 ? bw / 6 : 3, inner;
    bw = U * 13 / 10;
    x -= bw + nw;
    FillArea(x, by, bw, bh, WHITE);
    FillArea(x + bw, by + bh / 4, nw, bh - 2 * (bh / 4), WHITE);
    FillArea(x + 3, by + 3, bw - 6, bh - 6, BLACK);
    inner = (bw - 10) * pct / 100; if (inner < 2) inner = 2;
    FillArea(x + 5, by + 5, inner, bh - 10, WHITE);
    if (IsCharging()) {                       /* llamp: dos triangles oposats */
      int cx = x + bw / 2, cy = by + bh / 2, hh = bh / 2 - 4, k2;
      for (k2 = 0; k2 < hh; k2++) {
        int wv = (hh - k2) * 2 / 3 + 1;
        FillArea(cx - wv + 2, cy - k2 - 1, wv + 2, 1, DGRAY);
        FillArea(cx - 2, cy + k2, wv + 2, 1, DGRAY);
      }
    }
  }
  snprintf(t, sizeof(t), "%d%%", pct);
  SetFont(fSmall, WHITE); w = StringWidth(t);
  x -= w + U / 4;
  DrawTextRect(x, 0, w, SB, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  x -= U / 2;
  /* Wi-Fi: connectat = ralletes plenes segons el senyal; encès sense connexió = ralletes normals buides + ECO;
     apagat = ralletes buides i totes baixes + OFF */
  {
    int off = !(net & NET_WIFI) && !(net & NET_WIFIREADY), eco = !off && !(net & NET_WIFIREADY);
    const char *lab = off ? "OFF" : eco ? "ECO" : NULL;
    if (!off && !eco) { int q = GetNetSignalQuality(); bars = q < 0 ? 4 : (q * 4 + 4) / 5; if (bars < 1) bars = 1; if (bars > 4) bars = 4; }
    x -= 4 * sw + 3 * gap;
    for (i = 0; i < 4; i++) {
      int h = off ? ih / 4 : ih * (i + 1) / 4, bx = x + i * (sw + gap), by = iy + ih - h;
      if (h < 5) h = 5, by = iy + ih - h;
      if (lab) { FillArea(bx, by, sw, h, WHITE); if (sw > 4 && h > 4) FillArea(bx + 2, by + 2, sw - 4, h - 4, BLACK); }
      else FillArea(bx, by, sw, h, i < bars ? WHITE : DGRAY);
    }
    if (lab) { SetFont(fSmall, WHITE); w = StringWidth(lab); x -= w + U / 4; DrawTextRect(x, 0, w, SB, lab, ALIGN_RIGHT | VALIGN_MIDDLE); }
  }
  /* Fletxes de transferència: només es veuen mentre hi ha tràfic; la direcció activa en blanc i l'altra en gris. */
  if (netAct) {
    int ah = ih * 9 / 10, aw = ah * 4 / 5, ay = (SB - ah) / 2;
    x -= U / 2 + aw;
    icon_arrow(x + aw / 2, ay, ah, 0, (netAct & NETA_UP) ? WHITE : DGRAY);
    x -= U / 3 + aw;
    icon_arrow(x + aw / 2, ay, ah, 1, (netAct & NETA_DOWN) ? WHITE : DGRAY);
  }
}
static void draw(int full);
static int tick_redraw(void);
static void clock_tick(void) {
  time_t now = time(NULL);
  if (tick_redraw()) draw(0);
  else { statusbar(0); PartialUpdate(0, 0, SW, SB); }
  SetWeakTimer("busamb_clock", clock_tick, (int)(61 - now % 60) * 1000);
}
static void header(const char *title, const char *sub) {
  SetFont(fTitle, BLACK);
  DrawTextRect(U / 2, SB + U / 4, SW - U, U + U / 2, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fSmall, BLACK);
  DrawTextRect(U / 2, SB + U + U * 3 / 4, SW - U, U, sub ? sub : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  FillArea(0, SB + 3 * U - 4, SW, 4, BLACK);
}
#define TOP (SB + 3 * U + U / 2)
#define NAVH (2 * U + U / 4)
#define BOTTOM (SH - NAVH - U / 2)
static void footer(int page) {
  int y = SH - NAVH - U / 4, bw = (SW - U) / 3; char pg[40];
  if (pages < 2) return;
  if (page > 0) button(U / 2, y, bw, NAVH, "< Anterior", 0, A_PREV, 0);
  snprintf(pg, sizeof(pg), "%d / %d", page + 1, pages);
  SetFont(fText, BLACK);
  DrawTextRect(U / 2 + bw, y, SW - U - 2 * bw, NAVH, pg, ALIGN_CENTER | VALIGN_MIDDLE);
  if (page < pages - 1) button(SW - U / 2 - bw, y, bw, NAVH, "Següent >", 0, A_NEXT, 0);
}
static char gk[NOTELEN]; static void (*askFn)(const char *);
static void on_ask(char *text) {
  if (text) {
    char *s = text, *p; size_t n;
    while (*s == ' ') s++;
    for (p = s; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
    n = strlen(s); while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
    if (n > 0 && askFn) askFn(s);
  }
  draw(1);
}
static void ask(const char *title, const char *initial, void (*fn)(const char *)) {
  askFn = fn; snprintf(gk, sizeof(gk), "%s", initial ? initial : "");
  OpenKeyboard(title, gk, NOTELEN - 20, KBD_NORMAL, on_ask);
}
static void (*confFn)(void);
static void on_conf(int b) { if (b == 1 && confFn) confFn(); draw(1); }
static void confirm(const char *title, const char *text, const char *yes, void (*fn)(void)) { confFn = fn; Dialog(ICON_QUESTION, title, text, yes, "Cancel·la", on_conf); }
/* ═════════════ Apps instal·lades (carpeta «applications») ═════════════ */
#define APPDIR FLASHDIR "/applications"
static char extApps[40][200]; static int nExt; static char selfName[200] = "Escriptori.app";
static void ext_scan(void) {
  DIR *d = opendir(APPDIR); struct dirent *e; int i, j;
  nExt = 0;
  if (!d) return;
  while (nExt < 40 && (e = readdir(d))) {
    size_t n = strlen(e->d_name);
    if (n < 5 || n > 190 || e->d_name[0] == '.' || strcmp(e->d_name + n - 4, ".app") || !strcmp(e->d_name, selfName) || !strcmp(e->d_name, "Escriptori.app")) continue;
    snprintf(extApps[nExt++], sizeof(extApps[0]), "%s", e->d_name);
  }
  closedir(d);
  for (i = 1; i < nExt; i++) { char t[200]; memcpy(t, extApps[i], 200); for (j = i - 1; j >= 0 && strcasecmp(extApps[j], t) > 0; j--) memcpy(extApps[j + 1], extApps[j], 200); memcpy(extApps[j + 1], t, 200); }
}
static void ext_launch(int i) {
  char path[500];
  if (i < 0 || i >= nExt) return;
  snprintf(path, sizeof(path), APPDIR "/%s", extApps[i]);
  OpenBook(path, "", 0);
}
static unsigned long crc32_buf(const unsigned char *b, long n) {
  unsigned long c = 0xFFFFFFFFUL; long i; int k;
  for (i = 0; i < n; i++) { c ^= b[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1UL))); }
  return (c ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
}
/* Canvia l'indicador de transferència i refresca només la barra d'estat (sense animació: una sola actualització parcial). */
static void net_activity(int flags) {
  if (netAct == flags) return;
  netAct = flags;
  statusbar(0);
  PartialUpdate(0, 0, SW, SB);
}
static char *net_get_t(const char *url, int *outLen, int timeout) {
  int size = 0; void *buf; char *copy;
  if (!(QueryNetwork() & NET_CONNECTED)) NetConnect(NULL);
  if (!(QueryNetwork() & NET_CONNECTED)) return NULL;
  net_activity(NETA_DOWN);
  ShowHourglass();
  buf = QuickDownload(url, &size, timeout);
  HideHourglass();
  net_activity(0);
  if (!buf || size <= 0) { if (buf) free(buf); return NULL; }
  copy = (char *)malloc((size_t)size + 1);
  if (copy) { memcpy(copy, buf, (size_t)size); copy[size] = 0; }
  free(buf);
  if (outLen) *outLen = size;
  return copy;
}
