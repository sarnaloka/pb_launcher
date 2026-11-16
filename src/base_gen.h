/* Peces de dibuix del sistema base (extretes del codi original). */
/* ───────────── dibuix ───────────── */
static void add_hit(int x, int y, int w, int h, int act, int arg) {
  if (nhits < MAXHITS) { hits[nhits].x = x; hits[nhits].y = y; hits[nhits].w = w; hits[nhits].h = h; hits[nhits].act = act; hits[nhits].arg = arg; nhits++; }
}
static void button(int x, int y, int w, int h, const char *label, int inv, int act, int arg) {
  int r = h / 4 > U / 2 ? U / 2 : h / 4;
  if (inv) rcard(x, y, w, h, r, 0, BLACK, BLACK); else rcard(x, y, w, h, r, 2, BLACK, WHITE);
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
  FillArea(cx - r / 6, cy + r / 4, 2 * (r / 6) + 1, r / 2, WHITE);
}
static void icon_exit(int cx, int cy, int r, int col) {
  int i, t = r / 5 > 1 ? r / 5 : 2;
  for (i = -r * 3 / 4; i <= r * 3 / 4; i++) { FillArea(cx + i - t / 2, cy + i - t / 2, t + 1, t + 1, col); FillArea(cx + i - t / 2, cy - i - t / 2, t + 1, t + 1, col); }
}
/* Activitat de xarxa: bits que la barra d'estat mostra com a fletxes (baixada / pujada) al costat del Wi-Fi. */
enum { NETA_DOWN = 1, NETA_UP = 2 };
static int netAct;
/* ───── elements de la barra d'estat ─────
   La barra és una llista de StatusItem (vegeu mod.h) dibuixada de dreta a esquerra: els del sistema (hora, bateria,
   Wi-Fi, xarxa) i després els que ofereixen els paquets. Tocar la barra obre el tauler ràpid. */
static int si_time_w(int h) { (void)h; SetFont(fTitle, BLACK); return StringWidth("00:00"); }
static void si_time_d(int x, int y, int w, int h, int fg) {
  char t[16]; time_t now = time(NULL); struct tm *tm = localtime(&now);
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  SetFont(fTitle, fg); DrawTextRect(x, y, w, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
}
static int si_batt_w(int h) { (void)h; SetFont(fSmall, BLACK); return StringWidth("100%") + U / 4 + U * 13 / 10 + 8; }
static void si_batt_d(int x, int y, int w, int h, int fg) {
  char t[16]; int pct = GetBatteryPower(), bw = U * 13 / 10, nw = 6, bh = h / 2 * 7 / 10, by = y + (h - bh) / 2, tw, bx, inner;
  (void)w;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  snprintf(t, sizeof(t), "%d%%", pct);
  SetFont(fSmall, fg); tw = StringWidth("100%");
  DrawTextRect(x, y, tw, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  bx = x + tw + U / 4;
  rcard(bx, by, bw, bh, 4, 2, fg, WHITE);
  rfill(bx + bw, by + bh / 4, nw, bh - 2 * (bh / 4), 2, fg);
  inner = (bw - 8) * pct / 100; if (inner < 2) inner = 2;
  rfill(bx + 4, by + 4, inner, bh - 8, 2, fg);
  if (IsCharging()) {                          /* llamp: dos triangles oposats */
    int cx = bx + bw / 2, cy = by + bh / 2, hh = bh / 2 - 3, k2, col = inner > bw / 2 ? WHITE : fg;
    for (k2 = 0; k2 < hh; k2++) {
      int wv = (hh - k2) * 2 / 3 + 1;
      FillArea(cx - wv + 2, cy - k2 - 1, wv + 2, 1, col);
      FillArea(cx - 2, cy + k2, wv + 2, 1, col);
    }
  }
}
/* Wi-Fi: connectat = arcs plens segons el senyal; encès sense connexió = arcs en gris fosc; apagat = arcs clars i barrats */
static int si_wifi_w(int h) { return (h / 4) * 5 / 2 + 2; }
static void si_wifi_d(int x, int y, int w, int h, int fg) {
  int net = QueryNetwork(), off = !(net & NET_WIFI) && !(net & NET_WIFIREADY), eco = !off && !(net & NET_WIFIREADY), r = h / 4, level = 0;
  if (!off && !eco) { int q = GetNetSignalQuality(), bars = q < 0 ? 4 : (q * 4 + 4) / 5; if (bars < 1) bars = 1; if (bars > 4) bars = 4; level = (bars * 3 + 3) / 4; }
  ic_wifi(x + w / 2, y + h / 2 + r / 4, r, level, fg, off ? LGRAY : DGRAY, off);
}
static int si_net_w(int h) { int ah = h / 2 * 9 / 10; return netAct ? 2 * (ah * 4 / 5) + U / 3 : 0; }
static void si_net_d(int x, int y, int w, int h, int fg) {
  int ah = h / 2 * 9 / 10, aw = ah * 4 / 5, ay = y + (h - ah) / 2; (void)w;
  ic_arrow(x + aw / 2, ay, ah, 1, (netAct & NETA_DOWN) ? fg : LGRAY);
  ic_arrow(x + aw + U / 3 + aw / 2, ay, ah, 0, (netAct & NETA_UP) ? fg : LGRAY);
}
static int status_modules(const StatusItem **out, int max);
static void statusbar(int withHits) {
  static const StatusItem SYS[4] = { { "hora", si_time_w, si_time_d }, { "bateria", si_batt_w, si_batt_d }, { "wifi", si_wifi_w, si_wifi_d }, { "xarxa", si_net_w, si_net_d } };
  const StatusItem *list[40]; int n = 0, i, x = SW - U / 2, nb = SB * 5 / 4, r = SB / 4, act = depth > 1 ? BLACK : LGRAY;
  for (i = 0; i < 4; i++) list[n++] = &SYS[i];
  n += status_modules(list + n, 40 - n);
  FillArea(0, 0, SW, SB, WHITE); FillArea(0, SB - 2, SW, 2, BLACK);
  icon_back(nb / 2, SB / 2, r, act);
  icon_home(nb + nb / 2, SB / 2, r, act);
  icon_exit(2 * nb + nb / 2, SB / 2, r, BLACK);
  FillArea(nb, SB / 4, 1, SB / 2, LGRAY); FillArea(2 * nb, SB / 4, 1, SB / 2, LGRAY); FillArea(3 * nb, SB / 4, 1, SB / 2, LGRAY);
  if (withHits) {
    add_hit(3 * nb, 0, SW - 3 * nb, SB, A_QUICK, 0);
    if (depth > 1) { add_hit(0, 0, nb, SB, A_BACK, 0); add_hit(nb, 0, nb, SB, A_HOME, 0); }
    add_hit(2 * nb, 0, nb, SB, A_EXIT, 0);
  }
  for (i = 0; i < n; i++) {
    int w = list[i]->width(SB);
    if (w <= 0) continue;
    if (x - w < 3 * nb + U) break;
    x -= w; list[i]->draw(x, 0, w, SB, BLACK); x -= U / 2;
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
  SetFont(fSmall, DGRAY);
  DrawTextRect(U / 2, SB + U + U * 3 / 4, SW - U, U, sub ? sub : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  FillArea(U / 2, SB + 3 * U - 4, SW - U, 3, DGRAY);
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
