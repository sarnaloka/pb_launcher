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
/* ───── barra d'estat ─────
   Negra, amb tot en línia del mateix gruix i icones de la mateixa alçada (ICH), centrades en una sola línia.
   A l'esquerra, navegació en cel·les iguals; a la dreta, una llista d'elements amb una sola separació entre grups.
   Els elements són StatusItem (vegeu mod.h): els del sistema (hora, bateria, Wi-Fi, càrrega) i els dels paquets. */
#define ICH (SB * 46 / 100)                    /* alçada comuna de totes les icones (casella quadrada) */
#define NAVT (SB / 22 > 3 ? SB / 22 : 3)       /* gruix comú del traç de les icones de línia */
static void icon_back(int cx, int cy, int r, int col) {
  thick(cx - r, cy, cx + r, cy, NAVT, col);
  thick(cx - r, cy, cx - r / 4, cy - r * 3 / 4, NAVT, col); thick(cx - r, cy, cx - r / 4, cy + r * 3 / 4, NAVT, col);
}
static void icon_home(int cx, int cy, int r, int col) {
  thick(cx - r, cy - r / 8, cx, cy - r * 9 / 10, NAVT, col); thick(cx, cy - r * 9 / 10, cx + r, cy - r / 8, NAVT, col);
  thick(cx - r * 7 / 10, cy - r / 8, cx - r * 7 / 10, cy + r * 9 / 10, NAVT, col); thick(cx + r * 7 / 10, cy - r / 8, cx + r * 7 / 10, cy + r * 9 / 10, NAVT, col);
  thick(cx - r * 7 / 10, cy + r * 9 / 10, cx + r * 7 / 10, cy + r * 9 / 10, NAVT, col);
}
static void icon_exit(int cx, int cy, int r, int col) {
  thick(cx - r * 4 / 5, cy - r * 4 / 5, cx + r * 4 / 5, cy + r * 4 / 5, NAVT, col); thick(cx - r * 4 / 5, cy + r * 4 / 5, cx + r * 4 / 5, cy - r * 4 / 5, NAVT, col);
}
/* Activitat de xarxa: mentre és diferent de zero, la barra mostra un cercle de càrrega al costat del Wi-Fi.
   Els bits distingeixen baixada i pujada per si més endavant cal diferenciar-les. */
enum { NETA_DOWN = 1, NETA_UP = 2 };
static int netAct;
static int si_time_w(int h) { (void)h; SetFont(fBold, WHITE); return StringWidth("00:00"); }
static void si_time_d(int x, int y, int w, int h, int fg) {
  char t[16]; time_t now = time(NULL); struct tm *tm = localtime(&now);
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  SetFont(fBold, fg); DrawTextRect(x, y, w, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
}
static int batt_pct(void) { int p = GetBatteryPower(); return p < 0 ? 0 : p > 100 ? 100 : p; }
static int si_batt_w(int h) { char t[16]; (void)h; snprintf(t, sizeof(t), "%d%%", batt_pct()); SetFont(fSmall, WHITE); return StringWidth(t) + U / 4 + ICH * 3 / 2 + 5; }
static void si_batt_d(int x, int y, int w, int h, int fg) {
  char t[16]; int pct = batt_pct(), bh = ICH * 3 / 4, bw = bh * 2, nw = 3, by = y + (h - bh) / 2, tw, bx, inner;
  (void)w;
  snprintf(t, sizeof(t), "%d%%", pct);
  SetFont(fSmall, fg); tw = StringWidth(t);
  DrawTextRect(x, y, tw, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  bx = x + tw + U / 4;
  rcard(bx, by, bw, bh, 4, 3, fg, BLACK);
  rfill(bx + bw, by + bh / 4, nw, bh - 2 * (bh / 4), 1, fg);
  inner = (bw - 10) * pct / 100; if (inner < 2) inner = 2;
  rfill(bx + 5, by + 5, inner, bh - 10, 2, fg);
  if (IsCharging()) {                          /* llamp: dos triangles oposats, del color que contrasti amb el farciment */
    int cx = bx + bw / 2, cy = by + bh / 2, hh = bh / 2 - 3, k2, col = inner > bw / 2 ? BLACK : fg;
    for (k2 = 0; k2 < hh; k2++) {
      int wv = (hh - k2) * 2 / 3 + 1;
      FillArea(cx - wv + 2, cy - k2 - 1, wv + 2, 1, col);
      FillArea(cx - 2, cy + k2, wv + 2, 1, col);
    }
  }
}
/* Wi-Fi: barres de senyal. Connectat: barres segons el senyal. Encès sense connexió o apagat: barres apagades i una petita «x». */
static int wifi_level(void) {                  /* 0 = sense connexió; 1..4 = barres */
  int q, bars;
  if (!(QueryNetwork() & NET_WIFIREADY)) return 0;
  q = GetNetSignalQuality(); bars = q < 0 ? 4 : (q * 4 + 4) / 5;
  return bars < 1 ? 1 : bars > 4 ? 4 : bars;
}
static int si_wifi_w(int h) { (void)h; return ic_signal_w(ICH); }
static void si_wifi_d(int x, int y, int w, int h, int fg) {
  int lv = wifi_level();
  (void)w;
  ic_signal(x + ic_signal_w(ICH) / 2, y + h / 2, ICH, lv, fg, DGRAY, lv ? -1 : fg);
}
/* Avisos: campana i comptador de no llegits; no es mostra si no n'hi ha. */
static int avisos_unread(void);
static int si_av_w(int h) { char t[16]; int n = avisos_unread(); (void)h; if (n <= 0) return 0; snprintf(t, sizeof(t), "%d", n > 99 ? 99 : n); SetFont(fSmall, WHITE); return ICH + U / 4 + StringWidth(t); }
static void si_av_d(int x, int y, int w, int h, int fg) {
  char t[16]; int n = avisos_unread();
  (void)w; snprintf(t, sizeof(t), "%d", n > 99 ? 99 : n);
  ic_bell(x + ICH / 2, y + h / 2, ICH, fg, BLACK);
  SetFont(fSmall, fg); DrawTextRect(x + ICH + U / 4, y, StringWidth(t) + 2, h, t, ALIGN_LEFT | VALIGN_MIDDLE);
}
static int si_net_w(int h) { (void)h; return netAct ? ICH * 9 / 10 : 0; }
static void si_net_d(int x, int y, int w, int h, int fg) { ic_spinner(x + w / 2, y + h / 2, w / 2, fg, LGRAY, DGRAY); }
static int status_modules(const StatusItem **out, int max);
#define NSYS 5
/* Geometria i estat de cada peça de la barra: permet repintar i actualitzar només la peça que canvia. */
static int sbX[40], sbW[40], sbN, sbSig[NSYS], sbR0;
static int sb_sig(int i) {
  int n;
  switch (i) {
    case 0: return (int)(time(NULL) / 60 % 1000000);
    case 1: return batt_pct() * 2 + (IsCharging() ? 1 : 0);
    case 2: n = QueryNetwork(); return wifi_level() * 4 + ((n & NET_WIFI) ? 1 : 0) + ((n & NET_WIFIREADY) ? 2 : 0);
    case 3: return netAct;
    default: return avisos_unread();
  }
}
static void statusbar(int withHits) {
  static const StatusItem SYS[NSYS] = { { "hora", si_time_w, si_time_d }, { "bateria", si_batt_w, si_batt_d }, { "wifi", si_wifi_w, si_wifi_d }, { "xarxa", si_net_w, si_net_d }, { "avisos", si_av_w, si_av_d } };
  const StatusItem *list[40]; int n = 0, i, x = SW - U / 2, hx0 = 0, nb = SB * 11 / 10, r = ICH / 2, act = depth > 1 ? WHITE : DGRAY, gap = U * 7 / 10;
  for (i = 0; i < NSYS; i++) list[n++] = &SYS[i];
  n += status_modules(list + n, 40 - n);
  for (i = 0; i < 40; i++) { sbX[i] = 0; sbW[i] = 0; }
  sbN = n; sbR0 = U / 4 + 3 * nb;
  for (i = 0; i < NSYS; i++) sbSig[i] = sb_sig(i);
  FillArea(0, 0, SW, SB, BLACK);
  icon_back(U / 4 + nb / 2, SB / 2, r, act);
  icon_home(U / 4 + nb + nb / 2, SB / 2, r, act);
  icon_exit(U / 4 + 2 * nb + nb / 2, SB / 2, r, WHITE);
  if (withHits) {
    add_hit(U / 4 + 3 * nb, 0, SW - 3 * nb, SB, A_QUICK, 0);
    if (depth > 1) { add_hit(0, 0, U / 4 + nb, SB, A_BACK, 0); add_hit(U / 4 + nb, 0, nb, SB, A_HOME, 0); }
    add_hit(U / 4 + 2 * nb, 0, nb, SB, A_EXIT, 0);
  }
  for (i = 0; i < n; i++) {
    int w = list[i]->width(SB);
    if (w <= 0) continue;
    if (x - w < U / 4 + 3 * nb + U) break;
    x -= w; sbX[i] = x; sbW[i] = w; list[i]->draw(x, 0, w, SB, WHITE);
    if (i == 4 && withHits) { hx0 = x; add_hit(hx0 - gap / 2, 0, w + gap, SB, A_AVISOS, 0); }
    x -= gap;
  }
}
/* Repinta la barra a la memòria i actualitza la pantalla només a les peces que han canviat (cap refresc si no ha canviat res).
   Si una peça canvia d'amplada i la resta es desplaça, s'actualitza tota la zona dreta. */
static void sb_refresh(void) {
  int oSig[NSYS], oX[40], oW[40], oN = sbN, i, lay = 0, x0 = SW, x1 = 0, ch = 0;
  memcpy(oSig, sbSig, sizeof(oSig)); memcpy(oX, sbX, sizeof(oX)); memcpy(oW, sbW, sizeof(oW));
  statusbar(0);
  if (oN != sbN) lay = 1;
  for (i = 0; i < 40 && !lay; i++) if (oX[i] != sbX[i] || oW[i] != sbW[i]) lay = 1;
  if (lay) { x0 = sbR0; x1 = SW; ch = 1; }
  else for (i = 0; i < NSYS; i++) if (oSig[i] != sbSig[i] && sbW[i] > 0) {
    int a = sbX[i] - 3, b = sbX[i] + sbW[i] + 3;
    if (a < x0) x0 = a;
    if (b > x1) x1 = b;
    ch = 1;
  }
  if (!ch) return;
  if (x0 < 0) x0 = 0;
  if (x1 > SW) x1 = SW;
  PartialUpdate(x0, 0, x1 - x0, SB);
}
static void draw(int full);
static int tick_redraw(void);
static int desk_tick(void);
static void avisos_poll(void);
static void clock_tick(void) {
  time_t now = time(NULL);
  avisos_poll();
  if (tick_redraw()) { if (!desk_tick()) draw(0); }
  else sb_refresh();
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
  sb_refresh();
}
static char *net_get_t(const char *url, int *outLen, int timeout) {
  int size = 0; void *buf; char *copy;
  if (!(QueryNetwork() & NET_CONNECTED)) NetConnect(NULL);
  if (!(QueryNetwork() & NET_CONNECTED)) return NULL;
  net_activity(NETA_DOWN);                 /* única actualització pròpia (el cercle, un cop per operació); sense rellotge de sorra */
  buf = QuickDownload(url, &size, timeout);
  if (!buf || size <= 0) { if (buf) free(buf); return NULL; }
  copy = (char *)malloc((size_t)size + 1);
  if (copy) { memcpy(copy, buf, (size_t)size); copy[size] = 0; }
  free(buf);
  if (outLen) *outLen = size;
  return copy;
}
