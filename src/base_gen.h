/* Peces de dibuix del sistema base (extretes del codi original). */
/* ───────────── dibuix ───────────── */
static int inPane = -1;
static void add_hit(int x, int y, int w, int h, int act, int arg) {
  if (nhits < MAXHITS) { hits[nhits].x = x; hits[nhits].y = y + escOY; hits[nhits].w = w; hits[nhits].h = h; hits[nhits].act = act; hits[nhits].arg = arg; hits[nhits].pane = inPane; nhits++; }
}
static int escDark;                            /* tema fosc: la pantalla s'inverteix en acabar de dibuixar */
/* Píndola amb ombra (o plena amb el color d'accent): la forma comuna de botons i botons rodons. */
static void pill(int x, int y, int w, int h, int solid) {
  int r = h / 2;
  if (solid) { rfill(x, y, w, h, r, PAL(P_ACC)); return; }
  if (PAL(P_BG) == PAL(P_CARD)) { rcard(x, y, w, h, r, 3, PAL(P_LINE), PAL(P_CARD)); return; }
  rfill(x, y + 5, w, h, r, PAL(P_SHADOW)); rfill(x, y, w, h, r, PAL(P_CARD));
}
static void button(int x, int y, int w, int h, const char *label, int inv, int act, int arg) {
  pill(x, y, w, h, inv);
  SetFont(fBold, inv ? PAL(P_ACCFG) : PAL(P_FG));
  DrawTextRect(x + 4, y, w - 8, h, label, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
  add_hit(x, y, w, h, act, arg);
}
/* ───── barra d'estat ─────
   Integrada al fons (sense franja), amb icones d'una mateixa alçada (ICH) centrades en una sola línia.
   A l'esquerra, l'hora (a l'inici, el dia, perquè l'hora ja hi és gran); a la dreta, els indicadors.
   Els elements són StatusItem (vegeu mod.h): els del sistema (hora, bateria, Wi-Fi, càrrega) i els dels paquets. */
#define ICH (SB * 46 / 100)                    /* alçada comuna de totes les icones (casella quadrada) */
#define NAVT (SB / 20 > 3 ? SB / 20 : 3)       /* gruix comú del traç de les icones de línia */
static UNUSED void icon_back(int cx, int cy, int r, int col) {
  thick(cx - r, cy, cx + r, cy, NAVT, col);
  thick(cx - r, cy, cx - r / 4, cy - r * 3 / 4, NAVT, col); thick(cx - r, cy, cx - r / 4, cy + r * 3 / 4, NAVT, col);
}
static UNUSED void icon_home(int cx, int cy, int r, int col) {
  thick(cx - r, cy - r / 8, cx, cy - r * 9 / 10, NAVT, col); thick(cx, cy - r * 9 / 10, cx + r, cy - r / 8, NAVT, col);
  thick(cx - r * 7 / 10, cy - r / 8, cx - r * 7 / 10, cy + r * 9 / 10, NAVT, col); thick(cx + r * 7 / 10, cy - r / 8, cx + r * 7 / 10, cy + r * 9 / 10, NAVT, col);
  thick(cx - r * 7 / 10, cy + r * 9 / 10, cx + r * 7 / 10, cy + r * 9 / 10, NAVT, col);
}
static UNUSED void icon_exit(int cx, int cy, int r, int col) {
  thick(cx - r * 4 / 5, cy - r * 4 / 5, cx + r * 4 / 5, cy + r * 4 / 5, NAVT, col); thick(cx - r * 4 / 5, cy + r * 4 / 5, cx + r * 4 / 5, cy - r * 4 / 5, NAVT, col);
}
/* Activitat de xarxa: mentre és diferent de zero, la barra mostra un cercle de càrrega al costat del Wi-Fi.
   Els bits distingeixen baixada i pujada per si més endavant cal diferenciar-les. */
enum { NETA_DOWN = 1, NETA_UP = 2 };
static int netAct;
static int sb_on_desk(void);
static void si_time_txt(char *t, int sz) {
  static const char *WD[7] = { "diumenge", "dilluns", "dimarts", "dimecres", "dijous", "divendres", "dissabte" };
  time_t now = time(NULL); struct tm *tm = localtime(&now);
  if (sb_on_desk()) snprintf(t, (size_t)sz, "%s %d", WD[tm->tm_wday], tm->tm_mday); else snprintf(t, (size_t)sz, "%d:%02d", tm->tm_hour, tm->tm_min);
}
static int si_time_w(int h) { char t[40]; (void)h; si_time_txt(t, sizeof(t)); SetFont(fBold, PAL(P_FG)); return StringWidth(t) + U / 2; }
static void si_time_d(int x, int y, int w, int h, int fg) {
  char t[40]; si_time_txt(t, sizeof(t));
  SetFont(fBold, fg); DrawTextRect(x, y, w, h, t, ALIGN_LEFT | VALIGN_MIDDLE);
}
static int batt_pct(void) { int p = GetBatteryPower(); return p < 0 ? 0 : p > 100 ? 100 : p; }
static int si_batt_w(int h) { char t[16]; (void)h; snprintf(t, sizeof(t), "%d%%", batt_pct()); SetFont(fSmall, PAL(P_FG)); return StringWidth(t) + U / 4 + ICH * 3 / 2 + 5; }
static void si_batt_d(int x, int y, int w, int h, int fg) {
  char t[16]; int pct = batt_pct(), bh = ICH * 3 / 4, bw = bh * 2, nw = 3, by = y + (h - bh) / 2, tw, bx, inner;
  (void)w;
  snprintf(t, sizeof(t), "%d%%", pct);
  SetFont(fSmall, fg); tw = StringWidth(t);
  DrawTextRect(x, y, tw, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  bx = x + tw + U / 4;
  rcard(bx, by, bw, bh, 5, 3, fg, PAL(P_BG));
  rfill(bx + bw, by + bh / 4, nw, bh - 2 * (bh / 4), 1, fg);
  inner = (bw - 10) * pct / 100; if (inner < 2) inner = 2;
  rfill(bx + 5, by + 5, inner, bh - 10, 2, fg);
  if (IsCharging()) {                          /* llamp: dos triangles oposats, del color que contrasti amb el farciment */
    int cx = bx + bw / 2, cy = by + bh / 2, hh = bh / 2 - 3, k2, col = inner > bw / 2 ? PAL(P_BG) : fg;
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
  ic_signal(x + ic_signal_w(ICH) / 2, y + h / 2, ICH, lv, fg, PAL(P_SHADOW), lv ? -1 : fg);
}
/* Avisos: campana i comptador de no llegits; no es mostra si no n'hi ha. */
static int avisos_unread(void);
static int si_av_w(int h) { char t[16]; int n = avisos_unread(); (void)h; if (n <= 0) return 0; snprintf(t, sizeof(t), "%d", n > 99 ? 99 : n); SetFont(fSmall, PAL(P_FG)); return ICH + U / 4 + StringWidth(t); }
static void si_av_d(int x, int y, int w, int h, int fg) {
  char t[16]; int n = avisos_unread();
  (void)w; snprintf(t, sizeof(t), "%d", n > 99 ? 99 : n);
  ic_path(IC_BELL, x - ICH / 8, y + (h - ICH * 5 / 4) / 2, ICH * 5 / 4, 2.2, fg, PAL(P_BG));
  SetFont(fSmall, fg); DrawTextRect(x + ICH + U / 4, y, StringWidth(t) + 2, h, t, ALIGN_LEFT | VALIGN_MIDDLE);
}
static int jobs_progress(void);
/* Xarxa: amb feines a la cua, fletxa i barra de progrés; amb una descàrrega síncrona, el cercle. */
/* Apps obertes: icona i nombre; en tocar-la, la llista. */
static int open_count(void);
static int si_open_w(int h) { char t[8]; int n = open_count(); (void)h; if (!n) return 0; snprintf(t, sizeof(t), "%d", n); SetFont(fSmall, PAL(P_FG)); return ICH + U / 6 + StringWidth(t); }
static void si_open_d(int x, int y, int w, int h, int fg) {
  char t[8]; (void)w; snprintf(t, sizeof(t), "%d", open_count());
  ic_path(IC_APPS, x, y + (h - ICH) / 2, ICH, 2.2, fg, PAL(P_BG));
  SetFont(fSmall, fg); DrawTextRect(x + ICH + U / 6, y, StringWidth(t) + 2, h, t, ALIGN_LEFT | VALIGN_MIDDLE);
}
static int si_net_w(int h) { (void)h; if (jobs_progress() >= 0) return ICH * 3; return netAct ? ICH * 9 / 10 : 0; }
static void si_net_d(int x, int y, int w, int h, int fg) {
  int p = jobs_progress();
  if (p >= 0) {
    int bw = ICH * 2 - U / 4, bh = ICH / 3 > 6 ? ICH / 3 : 6, bx = x + ICH + U / 4, by = y + (h - bh) / 2;
    ic_path(IC_DOWNLOAD, x, y + (h - ICH) / 2, ICH, 2.4, fg, PAL(P_BG));
    rfill(bx, by, bw, bh, bh / 2, PAL(P_SHADOW));
    if (p <= 100 && p * bw / 100 > bh) rfill(bx, by, p * bw / 100, bh, bh / 2, fg);
    return;
  }
  ic_spinner(x + w / 2, y + h / 2, w / 2, fg, PAL(P_SUB), PAL(P_SHADOW));
}
static int status_modules(const StatusItem **out, int max);
#define NSYS 6
/* Geometria i estat de cada peça de la barra: permet repintar i actualitzar només la peça que canvia. */
static int sbX[40], sbW[40], sbN, sbSig[NSYS], sbR0;
static int sb_sig(int i) {
  int n;
  switch (i) {
    case 0: return sb_on_desk() ? (int)(time(NULL) / 86400 % 1000000) : (int)(time(NULL) / 60 % 1000000);
    case 1: return batt_pct() * 2 + (IsCharging() ? 1 : 0);
    case 2: n = QueryNetwork(); return wifi_level() * 4 + ((n & NET_WIFI) ? 1 : 0) + ((n & NET_WIFIREADY) ? 2 : 0);
    case 3: { int p = jobs_progress(); return netAct * 1000 + (p < 0 ? 999 : p / 10); }
    case 4: return avisos_unread();
    default: return open_count();
  }
}
static void statusbar(int withHits) {
  static const StatusItem SYS[NSYS] = { { "hora", si_time_w, si_time_d }, { "bateria", si_batt_w, si_batt_d }, { "wifi", si_wifi_w, si_wifi_d }, { "xarxa", si_net_w, si_net_d }, { "avisos", si_av_w, si_av_d }, { "obertes", si_open_w, si_open_d } };
  const StatusItem *list[40]; int n = 0, i, x = SW - U / 2, gap = U * 7 / 10, tw;
  for (i = 0; i < NSYS; i++) list[n++] = &SYS[i];
  n += status_modules(list + n, 40 - n);
  for (i = 0; i < 40; i++) { sbX[i] = 0; sbW[i] = 0; }
  for (i = 0; i < NSYS; i++) sbSig[i] = sb_sig(i);
  FillArea(0, 0, SW, SB, PAL(P_BG));
  /* la barra només mostra estat: hora a l'esquerra, indicadors a la dreta. Tota la barra obre el tauler ràpid. */
  tw = si_time_w(SB); sbX[0] = U / 2; sbW[0] = tw; si_time_d(U / 2, 0, tw, SB, PAL(P_FG));
  sbN = n; sbR0 = U / 2 + tw + U;
  if (withHits) add_hit(0, 0, SW, SB, A_QUICK, 0);
  for (i = 1; i < n; i++) {
    int w = list[i]->width(SB);
    if (w <= 0) continue;
    if (x - w < sbR0) break;
    x -= w; sbX[i] = x; sbW[i] = w; list[i]->draw(x, 0, w, SB, PAL(P_FG));
    if (i == 4 && withHits) add_hit(x - gap / 2, 0, w + gap, SB, A_AVISOS, 0);
    if (i == 5 && withHits) add_hit(x - gap / 2, 0, w + gap, SB, A_CFG_GO, V_OPEN);
    x -= gap;
  }
}
/* Repinta la barra a la memòria i actualitza la pantalla només a les peces que han canviat (cap refresc si no ha canviat res).
   Si una peça canvia d'amplada i la resta es desplaça, s'actualitza tota la zona dreta. */
static void pane_leave(void); static void pane_enter(int p);
static void sb_refresh_(void);
static void sb_refresh(void) { int inp = inPane; if (inp >= 0) pane_leave(); sb_refresh_(); if (inp >= 0) pane_enter(inp); }
static void sb_refresh_(void) {
  int oSig[NSYS], oX[40], oW[40], oN = sbN, i, lay = 0, x0 = SW, x1 = 0, ch = 0;
  memcpy(oSig, sbSig, sizeof(oSig)); memcpy(oX, sbX, sizeof(oX)); memcpy(oW, sbW, sizeof(oW));
  statusbar(0);
  if (escDark) InvertArea(0, 0, SW, SB);
  if (oN != sbN) lay = 1;
  for (i = 0; i < 40 && !lay; i++) if (oX[i] != sbX[i] || oW[i] != sbW[i]) lay = 1;
  if (lay || (oSig[0] != sbSig[0] && sbW[0] != oW[0])) { x0 = 0; x1 = SW; ch = 1; }
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
static void bg_tick(void);
static void clock_tick(void) {
  time_t now = time(NULL);
  avisos_poll();
  if (tick_redraw()) { if (!desk_tick()) draw(0); }
  else sb_refresh();
  bg_tick();
  SetWeakTimer("escriptori_clock", clock_tick, (int)(61 - now % 60) * 1000);
}
/* Botó rodó (contorn o ple) i xebró; dir = -1 esquerra, 1 dreta. */
/* Botó rodó amb una icona del prototip. state: 0 = normal (blanc amb ombra), 1 = ple (accent), 2 = inactiu (pla, gris). */
static void round_icon(int x, int y, int d, int state, const char *ic) {
  int fg = state == 1 ? PAL(P_ACCFG) : state == 2 ? PAL(P_SHADOW) : PAL(P_FG), bg = state == 1 ? PAL(P_ACC) : state == 2 ? PAL(P_TILE) : PAL(P_CARD), s = d * 52 / 100;
  if (state == 2) rfill(x, y, d, d, d / 2, PAL(P_TILE)); else pill(x, y, d, d, state == 1);
  ic_path(ic, x + (d - s) / 2, y + (d - s) / 2, s, 2.2, fg, bg);
}
/* Capçalera de pantalla: botó rodó d'enrere, títol (i subtítol) i botó rodó d'inici. Ocupa de SB a TOP. */
static ifont *fHead;
static void header(const char *title, const char *sub) {
  int d = 2 * U + U / 4, y0 = SB + (3 * U - d) / 2 + U / 8, nav = depth > 1, tx = nav ? U / 2 + d + U / 2 : U / 2, tw = SW - tx - (nav ? d + U : U / 2);
  if (nav) {
    round_icon(U / 2, y0, d, 0, IC_BACK);
    add_hit(0, SB, U / 2 + d + U / 4, 3 * U + U / 4, A_BACK, 0);
    round_icon(SW - U / 2 - d, y0, d, 0, IC_HOME);
    add_hit(SW - U / 2 - d - U / 4, SB, d + U / 2 + U / 4, 3 * U + U / 4, A_HOME, 0);
  }
  SetFont(fHead ? fHead : fTitle, PAL(P_FG));
  if (sub && sub[0]) {
    DrawTextRect(tx, y0 - U / 8, tw, d * 6 / 10, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(tx, y0 + d * 6 / 10 - U / 8, tw, d * 4 / 10, sub, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  } else DrawTextRect(tx, y0, tw, d, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
}
#define TOP (SB + 3 * U + U / 2)
#define NAVH (2 * U + U / 4)
#define BOTTOM (SH - NAVH - U / 2)
/* Peu de paginació: botons rodons d'anterior i següent i «N de M» al mig. footY el pot moure la vista (l'inici el posa sobre els preferits). */
static int footY;
static void footer(int page) {
  int d = NAVH, y = footY; char pg[40];
  if (pages < 2) return;
  round_icon(U / 2, y, d, page > 0 ? 0 : 2, IC_BACK);
  if (page > 0) add_hit(0, y - U / 4, SW / 3, d + U / 2, A_PREV, 0);
  snprintf(pg, sizeof(pg), "%d de %d", page + 1, pages);
  SetFont(fBold, PAL(P_FG)); DrawTextRect(SW / 3, y, SW / 3, d, pg, ALIGN_CENTER | VALIGN_MIDDLE);
  round_icon(SW - U / 2 - d, y, d, page < pages - 1 ? 1 : 2, IC_FORWARD);
  if (page < pages - 1) add_hit(SW - SW / 3, y - U / 4, SW / 3, d + U / 2, A_NEXT, 0);
}
static char gk[NOTELEN]; static void (*askFn)(const char *);
static int uiModal;                     /* teclat o diàleg del sistema obert: les feines de fons no redibuixen */
static void on_ask(char *text) {
  uiModal = 0;
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
  askFn = fn; uiModal = 1; snprintf(gk, sizeof(gk), "%s", initial ? initial : "");
  OpenKeyboard(title, gk, NOTELEN - 20, KBD_NORMAL, on_ask);
}
static void (*confFn)(void);
static void on_conf(int b) { uiModal = 0; if (b == 1 && confFn) confFn(); draw(1); }
static void confirm(const char *title, const char *text, const char *yes, void (*fn)(void)) { confFn = fn; uiModal = 1; Dialog(ICON_QUESTION, title, text, yes, "Cancel·la", on_conf); }
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
