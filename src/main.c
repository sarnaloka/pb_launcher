/* Bus AMB — aplicació nativa per a PocketBook (inkview). Interfície. */
#include "inkview.h"
#include "amb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

extern const unsigned char amb_blob[], amb_blob_end[];

enum { V_HOME, V_SEARCH, V_LINES, V_LINE, V_LINESTOPS, V_STOP, V_TTPICK, V_TT, V_DESK, V_CLOCK, V_NOTES, V_NOTE, V_CALC,
       V_FILES, V_FILE, V_TODO, V_CONV, V_AGENDA, V_DAY, V_SUDOKU, V_MINES, V_WEATHER, V_RSS, V_RSSITEM, V_FEEDS, V_UPDATE };
enum { A_NONE, A_BACK, A_HOME, A_PREV, A_NEXT, A_SEARCH, A_LINES, A_LINE, A_PATTERN, A_STOP, A_FILTER, A_REFRESH,
       A_FAV, A_TTPICK, A_TT, A_TAB, A_PDF, A_EXIT, A_APP, A_NOTE_NEW, A_NOTE_OPEN, A_NOTE_EDIT, A_NOTE_DEL, A_CALC,
       A_FOPEN, A_FACT, A_FMOVEHERE, A_FMOVECANCEL, A_TODO_NEW, A_TODO_TOGGLE, A_TODO_CLEAR, A_CONV_CAT, A_CONV_UNIT, A_CONV_VAL,
       A_AG_MONTH, A_AG_DAY, A_AG_NEW, A_AG_DEL, A_SU_CELL, A_SU_NUM, A_SU_NEW, A_SU_CHECK, A_MI_CELL, A_MI_MODE, A_MI_NEW,
       A_WX_REFRESH, A_RSS_REFRESH, A_RSS_OPEN, A_RSS_FEEDS, A_FEED_ADD, A_FEED_DEL, A_UPD_CHECK, A_UPD_INSTALL };

typedef struct { int type, a, b, page, sel; char term[64]; } View;
typedef struct { int x, y, w, h, act, arg; } Hit;

#define MAXSTACK 24
#define MAXHITS 400
static View stack[MAXSTACK];
static int depth;
static Hit hits[MAXHITS];
static int nhits;
static int SW, SH, U, SB;                 /* amplada, alçada, unitat de mida, alçada de la barra d'estat */
static ifont *fTitle, *fBold, *fText, *fSmall, *fBig, *fHuge;
static int loadErr;
static int pages = 1;                      /* pàgines de la vista actual */
static char kbuf[64];
static char pdfPath[512];

static int *favs; static int nfavs;
static Group *cg; static int cgN, cgStop = -1;
static Timetable ctt; static int cttStop = -1, cttGi = -1;

#define FAVFILE FLASHDIR "/system/config/busamb_favs.txt"
#define PDFDIR  FLASHDIR "/Horaris AMB"

/* ───────────── preferides ───────────── */
static int fav_index(int stop) { int i; for (i = 0; i < nfavs; i++) if (favs[i] == stop) return i; return -1; }
static void favs_save(void) {
  FILE *f = fopen(FAVFILE, "w"); int i;
  if (!f) return;
  for (i = 0; i < nfavs; i++) fprintf(f, "%s\t%s\n", db.sCode[favs[i]], db.sName[favs[i]]);
  fclose(f);
}
static void favs_load(void) {
  FILE *f = fopen(FAVFILE, "r"); char ln[400]; int i;
  favs = (int *)malloc(sizeof(int) * 64); nfavs = 0;
  if (!f || !favs) { if (f) fclose(f); return; }
  while (fgets(ln, sizeof(ln), f) && nfavs < 64) {
    char *tab = strchr(ln, '\t'), *nl = strchr(ln, '\n'); const char *name;
    if (nl) *nl = 0;
    if (!tab) continue;
    *tab = 0; name = tab + 1;
    for (i = 0; i < db.nS; i++) if (!strcmp(db.sCode[i], ln) && !strcmp(db.sName[i], name)) { favs[nfavs++] = i; break; }
  }
  fclose(f);
}

/* ───────────── dibuix ───────────── */
static void add_hit(int x, int y, int w, int h, int act, int arg) {
  if (nhits < MAXHITS) { hits[nhits].x = x; hits[nhits].y = y; hits[nhits].w = w; hits[nhits].h = h; hits[nhits].act = act; hits[nhits].arg = arg; nhits++; }
}
static void box(int x, int y, int w, int h) { DrawRect(x, y, w, h, BLACK); DrawRect(x + 1, y + 1, w - 2, h - 2, BLACK); }
static void button(int x, int y, int w, int h, const char *label, int inv, int act, int arg) {
  if (inv) FillArea(x, y, w, h, BLACK); else box(x, y, w, h);
  SetFont(fBold, inv ? WHITE : BLACK);
  DrawTextRect(x + 4, y, w - 8, h, label, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
  add_hit(x, y, w, h, act, arg);
}
static void routes_text(int stop, char *out, int outsz) {
  int k, n = 0;
  out[0] = 0;
  for (k = db.srStart[stop]; k < db.srStart[stop + 1]; k++) {
    const char *l = amb_route_label(db.srList[k]); int len = (int)strlen(l);
    if (n + len + 3 >= outsz) break;
    if (n) { out[n++] = ' '; out[n++] = ' '; }
    memcpy(out + n, l, (size_t)len); n += len; out[n] = 0;
  }
}
static void stop_row(int y, int h, int stop) {
  char sub[600], rt[500];
  box(0 + U / 2, y, SW - U, h - U / 4);
  SetFont(fBold, BLACK);
  DrawTextRect(U, y + U / 6, SW - 2 * U, h / 2, db.sName[stop], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  routes_text(stop, rt, sizeof(rt));
  if (db.sCode[stop][0]) snprintf(sub, sizeof(sub), "Codi %s   %s", db.sCode[stop], rt); else snprintf(sub, sizeof(sub), "%s", rt);
  SetFont(fSmall, BLACK);
  DrawTextRect(U, y + h / 2, SW - 2 * U, h / 2 - U / 4, sub, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  add_hit(U / 2, y, SW - U, h - U / 4, A_STOP, stop);
}
/* Icones de navegació dibuixades amb rectangles (no depenen de cap tipografia). */
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
}
static void draw(int full);
static void clock_tick(void) {
  time_t now = time(NULL);
  if (depth > 0 && (stack[depth - 1].type == V_DESK || stack[depth - 1].type == V_CLOCK)) draw(0);
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

/* ───────────── vistes ───────────── */
static void fmt_date(char *out, int sz, time_t t) { struct tm *tm = localtime(&t); snprintf(out, (size_t)sz, "%02d/%02d/%d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900); }

static void v_home(View *v) {
  char sub[100], d[32]; int y = TOP, bh = 2 * U + U / 2, rh = 3 * U, per, i;
  time_t made = (time_t)db.made;
  fmt_date(d, sizeof(d), made);
  snprintf(sub, sizeof(sub), "Horari teòric · dades del %s", d);
  header("Bus AMB", sub);
  button(U / 2, y, SW - U, bh, "Cerca una parada", 0, A_SEARCH, 0); y += bh + U / 3;
  button(U / 2, y, SW - U, bh, "Tria per línia", 0, A_LINES, 0); y += bh + U / 2;
  if (nfavs) {
    SetFont(fBold, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, "Preferides", ALIGN_LEFT | VALIGN_MIDDLE); y += U + U / 2;
    per = (BOTTOM - y) / rh; if (per < 1) per = 1;
    pages = (nfavs + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
    for (i = v->page * per; i < nfavs && i < (v->page + 1) * per; i++) { stop_row(y, rh, favs[i]); y += rh; }
  } else {
    SetFont(fSmall, BLACK);
    DrawTextRect(U / 2, y, SW - U, 3 * U, "Obre una parada i prem «Preferida» per tenir-la aquí.", ALIGN_LEFT | VALIGN_TOP);
  }
}
static const char *g_w0;
static int cmp_hit(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b;
  long px = strstr(db.sQ[x], g_w0) - db.sQ[x], py = strstr(db.sQ[y], g_w0) - db.sQ[y];
  if (px != py) return px < py ? -1 : 1;
  return strcmp(db.sQ[x], db.sQ[y]);
}
static void v_search(View *v) {
  char q[128], *words[8], sub[64]; int nw = 0, i, k, n = 0, rh = 3 * U, per = (BOTTOM - TOP) / rh, y = TOP, shown = 0;
  char title[128], *p;
  int *res = (int *)malloc(sizeof(int) * (size_t)db.nS);
  amb_fold(v->term, q, sizeof(q));
  for (p = strtok(q, " "); p && nw < 8; p = strtok(NULL, " ")) words[nw++] = p;
  if (res) for (i = 0; i < db.nS; i++) {
    int ok = nw > 0;
    if (db.srStart[i + 1] == db.srStart[i]) continue;
    for (k = 0; ok && k < nw; k++) if (!strstr(db.sQ[i], words[k])) ok = 0;
    if (ok) res[n++] = i;
  }
  if (n > 1 && nw > 0) { g_w0 = words[0]; qsort(res, (size_t)n, sizeof(int), cmp_hit); }
  pages = n ? (n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  snprintf(title, sizeof(title), "Cerca: %s", v->term);
  snprintf(sub, sizeof(sub), n == 1 ? "%d parada" : "%d parades", n);
  header(title, sub);
  for (i = v->page * per; res && i < n && shown < per; i++, shown++) { stop_row(y, rh, res[i]); y += rh; }
  if (!n) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Cap parada trobada.", ALIGN_LEFT | VALIGN_TOP); }
  free(res);
}
static void v_lines(View *v) {
  int cols = 5, gap = U / 4, bw = (SW - U - (cols - 1) * gap) / cols, bh = 2 * U, rows = (BOTTOM - TOP) / (bh + gap), per = rows * cols, i, k = 0;
  char sub[40];
  pages = (db.nR + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  snprintf(sub, sizeof(sub), "%d línies", db.nR);
  header("Línies", sub);
  for (i = v->page * per; i < db.nR && k < per; i++, k++) {
    int r = db.routeOrder[i];
    button(U / 2 + (k % cols) * (bw + gap), TOP + (k / cols) * (bh + gap), bw, bh, amb_route_label(r), 0, A_LINE, r);
  }
}
/* sentits d'una línia: el viatge més llarg de cada (sentit, destí) */
typedef struct { int trip, n; const char *dest; int dir; } Pattern;
static Pattern pats[32]; static int npats, patsRoute = -1;
static void load_patterns(int r) {
  int t, i, j;
  if (patsRoute == r) return;
  npats = 0; patsRoute = r;
  for (t = 0; t < db.nT; t++) {
    const char *d;
    if (db.tRoute[t] != r || db.tStart[t] < 0) continue;
    d = amb_trip_dest(t);
    for (i = 0; i < npats; i++) if (pats[i].dir == db.tDir[t] && !strcmp(pats[i].dest, d)) break;
    if (i == npats) { if (npats == 32) continue; pats[npats].trip = t; pats[npats].n = 0; pats[npats].dest = d; pats[npats].dir = db.tDir[t]; npats++; }
    else if (db.tLen[t] > db.tLen[pats[i].trip]) pats[i].trip = t;
    pats[i].n++;
  }
  for (i = 1; i < npats; i++) { Pattern p = pats[i]; for (j = i - 1; j >= 0 && pats[j].n < p.n; j--) pats[j + 1] = pats[j]; pats[j + 1] = p; }
}
static void v_line(View *v) {
  char title[80], lab[300], sub[80]; int i, y = TOP, rh = 3 * U, per = (BOTTOM - TOP - 2 * U) / rh;
  load_patterns(v->a);
  pages = npats ? (npats + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  snprintf(title, sizeof(title), "Línia %s", amb_route_label(v->a));
  header(title, db.rLong[v->a]);
  SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, "Tria el sentit:", ALIGN_LEFT | VALIGN_MIDDLE); y += 2 * U;
  for (i = v->page * per; i < npats && i < (v->page + 1) * per; i++) {
    box(U / 2, y, SW - U, rh - U / 4);
    snprintf(lab, sizeof(lab), "Cap a %s", pats[i].dest);
    SetFont(fBold, BLACK); DrawTextRect(U, y + U / 6, SW - 2 * U, rh / 2, lab, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    snprintf(sub, sizeof(sub), "%d viatges · %d parades", pats[i].n, db.tLen[pats[i].trip]);
    SetFont(fSmall, BLACK); DrawTextRect(U, y + rh / 2, SW - 2 * U, rh / 2 - U / 4, sub, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_PATTERN, i);
    y += rh;
  }
}
static void v_linestops(View *v) {
  char title[300], sub[40]; int rh = 3 * U, per = (BOTTOM - TOP) / rh, y = TOP, i, a, n;
  load_patterns(v->a);
  if (v->b >= npats) return;
  a = db.tStart[pats[v->b].trip]; n = db.tLen[pats[v->b].trip];
  pages = (n + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  snprintf(title, sizeof(title), "%s cap a %s", amb_route_label(v->a), pats[v->b].dest);
  snprintf(sub, sizeof(sub), "%d parades", n);
  header(title, sub);
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) { stop_row(y, rh, db.sStop[a + i]); y += rh; }
}
static void v_stop(View *v) {
  int stop = v->a, y = TOP, k, nd, i, n = 0, later = 0, rh = 2 * U, per, x, ch = U + U * 3 / 4, nr = db.srStart[stop + 1] - db.srStart[stop], ah = 2 * U + U / 4, bw;
  time_t now = time(NULL); struct tm *tm = localtime(&now);
  char sub[120], t[16], m[24], lab[300];
  Dep *d = NULL;
  if (db.sCode[stop][0]) snprintf(sub, sizeof(sub), "Codi %s · consultat a les %02d:%02d", db.sCode[stop], tm->tm_hour, tm->tm_min);
  else snprintf(sub, sizeof(sub), "Consultat a les %02d:%02d", tm->tm_hour, tm->tm_min);
  header(db.sName[stop], sub);
  if (nr > 1) {                                   /* filtre per línia */
    x = U / 2;
    SetFont(fBold, BLACK);
    for (k = -1; k < nr; k++) {
      int r = k < 0 ? -1 : db.srList[db.srStart[stop] + k]; const char *l = k < 0 ? "Totes" : amb_route_label(r);
      int w = StringWidth(l) + U;
      if (x + w > SW - U / 2) { x = U / 2; y += ch + U / 4; }
      button(x, y, w, ch, l, v->sel == r, A_FILTER, r);
      SetFont(fBold, BLACK);
      x += w + U / 4;
    }
    y += ch + U / 2;
  }
  nd = amb_departures(stop, now, now - 60, now + 3 * 3600, &d);
  if (!nd) { free(d); d = NULL; nd = amb_departures(stop, now, now, now + 26 * 3600, &d); later = 1; }
  for (i = 0; i < nd; i++) if (v->sel < 0 || d[i].route == v->sel) d[n++] = d[i];
  per = (BOTTOM - ah - U / 2 - y - (later ? U + U / 2 : 0)) / rh; if (per < 1) per = 1;
  pages = n ? (n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (later && n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 4, "Cap sortida en les properes 3 hores. Properes:", ALIGN_LEFT | VALIGN_MIDDLE | DOTS); y += U + U / 2; }
  if (!n) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Cap sortida prevista.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    struct tm *dt = localtime(&d[i].when); int mins = (int)((d[i].when - now + 30) / 60), tw, lw;
    snprintf(t, sizeof(t), "%02d:%02d", dt->tm_hour, dt->tm_min);
    if (mins < 1) snprintf(m, sizeof(m), "ara"); else if (mins < 60) snprintf(m, sizeof(m), "%d min", mins); else m[0] = 0;
    SetFont(fBig, BLACK); tw = 4 * U; DrawTextRect(U / 2, y, tw, rh, t, ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fBold, WHITE); lw = StringWidth(amb_route_label(d[i].route)) + U / 2;
    FillArea(U / 2 + tw, y + rh / 6, lw, rh * 2 / 3, BLACK);
    DrawTextRect(U / 2 + tw, y + rh / 6, lw, rh * 2 / 3, amb_route_label(d[i].route), ALIGN_CENTER | VALIGN_MIDDLE);
    snprintf(lab, sizeof(lab), "%s", amb_trip_dest(d[i].trip));
    SetFont(fText, BLACK); DrawTextRect(U / 2 + tw + lw + U / 3, y, SW - U - tw - lw - U / 3 - 4 * U, rh, lab, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fText, BLACK); DrawTextRect(SW - U / 2 - 4 * U, y, 4 * U, rh, m, ALIGN_RIGHT | VALIGN_MIDDLE);
    DrawLine(U / 2, y + rh - 1, SW - U / 2, y + rh - 1, LGRAY);
    y += rh;
  }
  free(d);
  y = BOTTOM - ah; bw = (SW - U - 2 * (U / 3)) / 3;
  button(U / 2, y, bw, ah, "Actualitza", 0, A_REFRESH, 0);
  button(U / 2 + bw + U / 3, y, bw, ah, "Horari", 0, A_TTPICK, stop);
  button(U / 2 + 2 * (bw + U / 3), y, bw, ah, fav_index(stop) >= 0 ? "Preferida: sí" : "Preferida: no", fav_index(stop) >= 0, A_FAV, stop);
}
static void need_groups(int stop) { if (cgStop != stop) { amb_free_groups(cg, cgN); cg = NULL; cgN = amb_lines_at_stop(stop, &cg); cgStop = stop; cttStop = -1; } }
static void v_ttpick(View *v) {
  int y = TOP, rh = 2 * U + U / 2, per = (BOTTOM - TOP - 2 * U) / rh, i;
  need_groups(v->a);
  pages = cgN ? (cgN + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  header("Horari complet", db.sName[v->a]);
  SetFont(fText, BLACK);
  DrawTextRect(U / 2, y, SW - U, U + U / 2, cgN ? "Tria la línia i el sentit:" : "Aquesta parada no té sortides.", ALIGN_LEFT | VALIGN_MIDDLE); y += 2 * U;
  for (i = v->page * per; i < cgN && i < (v->page + 1) * per; i++) {
    int lw;
    box(U / 2, y, SW - U, rh - U / 4);
    SetFont(fBold, WHITE); lw = StringWidth(amb_route_label(cg[i].route)) + U / 2;
    FillArea(U, y + rh / 6, lw, rh / 2, BLACK);
    DrawTextRect(U, y + rh / 6, lw, rh / 2, amb_route_label(cg[i].route), ALIGN_CENTER | VALIGN_MIDDLE);
    SetFont(fBold, BLACK); DrawTextRect(U + lw + U / 3, y, SW - 2 * U - lw - U / 3, rh - U / 4, cg[i].dest, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_TT, i);
    y += rh;
  }
}
static const char *TABS[3] = { "Feiners", "Dissabte", "Festius" };
static void v_tt(View *v) {
  char title[300], sub[300], line[256], cell[16]; int y = TOP, c = v->sel, i, k, th = 2 * U, bw, lh = U + U / 3, hw = 2 * U, maxw, ah = 2 * U + U / 4, areaBottom, pg = 0, started;
  need_groups(v->a);
  if (v->b >= cgN) return;
  if (cttStop != v->a || cttGi != v->b) { if (cttStop >= 0) amb_free_timetable(&ctt); amb_timetable(&cg[v->b], time(NULL), &ctt); cttStop = v->a; cttGi = v->b; }
  if (c < 0 || c > 2) c = v->sel = 0;
  snprintf(title, sizeof(title), "%s cap a %s", amb_route_label(ctt.route), ctt.dest ? ctt.dest : "");
  snprintf(sub, sizeof(sub), "Parada: %s", db.sName[v->a]);
  header(title, sub);
  bw = (SW - U - 2 * (U / 4)) / 3;
  for (k = 0; k < 3; k++) button(U / 2 + k * (bw + U / 4), y, bw, th, TABS[k], k == c, A_TAB, k);
  y += th + U / 4;
  snprintf(sub, sizeof(sub), "Horari del %02d/%02d/%d", ctt.cols[c].d, ctt.cols[c].m, ctt.cols[c].y);
  SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 4, sub, ALIGN_LEFT | VALIGN_MIDDLE); y += U + U / 2;
  areaBottom = BOTTOM - ah - U / 2;
  maxw = SW - U - hw - U / 2;
  /* files per hora; les pàgines es calculen pel recorregut complet */
  started = y; i = 0; pages = 1;
  if (!ctt.cols[c].n) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Sense sortides aquest dia.", ALIGN_LEFT | VALIGN_TOP); }
  while (i < ctt.cols[c].n) {
    int h = ctt.cols[c].deps[i].time / 3600, j = i, nl = 1, w = 0, rowh, x, ly;
    SetFont(fText, BLACK);
    while (j < ctt.cols[c].n && ctt.cols[c].deps[j].time / 3600 == h) {       /* compta línies */
      int cw;
      snprintf(cell, sizeof(cell), "%02d%c ", (ctt.cols[c].deps[j].time % 3600) / 60, ctt.cols[c].deps[j].note ? ctt.cols[c].deps[j].note : ' ');
      cw = StringWidth(cell);
      if (w + cw > maxw && w > 0) { nl++; w = 0; }
      w += cw; j++;
    }
    rowh = nl * lh + U / 4;
    if (y + rowh > areaBottom && y > started) { pg++; y = started; }
    if (pg == v->page) {
      snprintf(line, sizeof(line), "%d", h % 24);
      SetFont(fBold, BLACK); DrawTextRect(U / 2, y, hw - U / 4, lh, line, ALIGN_RIGHT | VALIGN_MIDDLE);
      SetFont(fText, BLACK); x = U / 2 + hw + U / 4; ly = y;
      for (k = i; k < j; k++) {
        int cw;
        snprintf(cell, sizeof(cell), "%02d%c ", (ctt.cols[c].deps[k].time % 3600) / 60, ctt.cols[c].deps[k].note ? ctt.cols[c].deps[k].note : ' ');
        cw = StringWidth(cell);
        if (x + cw > U / 2 + hw + U / 4 + maxw && x > U / 2 + hw + U / 4) { x = U / 2 + hw + U / 4; ly += lh; }
        DrawTextRect(x, ly, cw + 4, lh, cell, ALIGN_LEFT | VALIGN_MIDDLE);
        x += cw;
      }
      DrawLine(U / 2, y + rowh - 1, SW - U / 2, y + rowh - 1, LGRAY);
    }
    y += rowh; i = j;
  }
  pages = pg + 1;
  if (v->page >= pages) v->page = pages - 1;
  y = BOTTOM - ah;
  if (ctt.nNotes) {
    line[0] = 0;
    for (k = 0; k < ctt.nNotes && strlen(line) < sizeof(line) - 180; k++) { char one[200]; snprintf(one, sizeof(one), "%s%c = %s", k ? "   " : "", ctt.notes[k].letter, ctt.notes[k].text); strncat(line, one, sizeof(line) - strlen(line) - 1); }
    SetFont(fSmall, BLACK); DrawTextRect(U / 2, y - U - U / 2, SW - U, U + U / 4, line, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  }
  button(U / 2, y, SW - U, ah, "Desa l'horari en PDF", 0, A_PDF, 0);
}

/* ═════════════ Escriptori i mini-apps ═════════════ */
static const char *WDAY[7] = { "diumenge", "dilluns", "dimarts", "dimecres", "dijous", "divendres", "dissabte" };
static const char *MONTH[12] = { "de gener", "de febrer", "de març", "d'abril", "de maig", "de juny", "de juliol", "d'agost", "de setembre", "d'octubre", "de novembre", "de desembre" };
static const char *MONTHN[12] = { "Gener", "Febrer", "Març", "Abril", "Maig", "Juny", "Juliol", "Agost", "Setembre", "Octubre", "Novembre", "Desembre" };
static void disc(int cx, int cy, int r, int col) {
  int dy;
  for (dy = -r; dy <= r; dy++) { int h = (int)(sqrt((double)(r * r - dy * dy)) + 0.5); if (h > 0) FillArea(cx - h, cy + dy, 2 * h, 1, col); }
}
static void thick(int x1, int y1, int x2, int y2, int t, int col) {
  int dx = x2 - x1, dy = y2 - y1, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy), i;
  if (n < 1) n = 1;
  for (i = 0; i <= n; i++) FillArea(x1 + dx * i / n - t / 2, y1 + dy * i / n - t / 2, t, t, col);
}
static void app_icon(int k, int x, int y, int s) {
  int i, j;
  switch (k) {
    case 0:   /* bus */
      FillArea(x + s / 8, y + s / 6, s * 3 / 4, s * 9 / 16, BLACK);
      for (i = 0; i < 3; i++) FillArea(x + s / 8 + s / 16 + i * (s * 7 / 32), y + s / 6 + s / 12, s * 5 / 32, s / 5, WHITE);
      FillArea(x + s / 8, y + s / 6 + s * 3 / 8, s * 3 / 4, s / 32 > 1 ? s / 32 : 2, WHITE);
      disc(x + s * 5 / 16, y + s * 3 / 4, s / 10, BLACK); disc(x + s * 11 / 16, y + s * 3 / 4, s / 10, BLACK);
      disc(x + s * 5 / 16, y + s * 3 / 4, s / 24, WHITE); disc(x + s * 11 / 16, y + s * 3 / 4, s / 24, WHITE);
      break;
    case 1:   /* rellotge */
      disc(x + s / 2, y + s / 2, s * 3 / 8, BLACK); disc(x + s / 2, y + s / 2, s * 3 / 8 - s / 16, WHITE);
      thick(x + s / 2, y + s / 2, x + s / 2, y + s / 4, s / 20 > 2 ? s / 20 : 3, BLACK);
      thick(x + s / 2, y + s / 2, x + s * 11 / 16, y + s * 9 / 16, s / 20 > 2 ? s / 20 : 3, BLACK);
      break;
    case 2:   /* notes */
      FillArea(x + s / 4, y + s / 8, s / 2, s * 3 / 4, BLACK); FillArea(x + s / 4 + 4, y + s / 8 + 4, s / 2 - 8, s * 3 / 4 - 8, WHITE);
      for (i = 0; i < 4; i++) FillArea(x + s / 4 + s / 12, y + s / 4 + i * (s / 8), s / 2 - s / 6, s / 32 > 1 ? s / 32 : 2, BLACK);
      break;
    case 3:   /* calculadora */
      FillArea(x + s / 4, y + s / 8, s / 2, s * 3 / 4, BLACK);
      FillArea(x + s / 4 + s / 16, y + s / 8 + s / 16, s / 2 - s / 8, s / 6, WHITE);
      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) FillArea(x + s / 4 + s / 16 + j * (s / 8 + s / 64), y + s / 8 + s * 5 / 16 + i * (s / 8 + s / 64), s / 10, s / 10, WHITE);
      break;
    case 4:   /* carpeta */
      FillArea(x + s / 8, y + s / 4, s / 3, s / 8, BLACK); FillArea(x + s / 8, y + s / 4 + s / 10, s * 3 / 4, s / 2, BLACK);
      break;
    case 5:   /* tasques */
      for (i = 0; i < 3; i++) {
        int yy = y + s / 5 + i * (s / 4);
        FillArea(x + s / 6, yy, s / 6, s / 6, BLACK); if (i > 0) FillArea(x + s / 6 + 3, yy + 3, s / 6 - 6, s / 6 - 6, WHITE);
        FillArea(x + s / 6 + s / 4, yy + s / 16, s / 2 - s / 12, s / 20 > 2 ? s / 20 : 3, BLACK);
      }
      break;
    case 6:   /* conversor: dues fletxes oposades */
      thick(x + s / 6, y + s * 3 / 8, x + s * 5 / 6, y + s * 3 / 8, s / 16 > 3 ? s / 16 : 4, BLACK);
      thick(x + s * 5 / 6, y + s * 3 / 8, x + s * 2 / 3, y + s / 4, s / 16 > 3 ? s / 16 : 4, BLACK);
      thick(x + s / 6, y + s * 5 / 8, x + s * 5 / 6, y + s * 5 / 8, s / 16 > 3 ? s / 16 : 4, BLACK);
      thick(x + s / 6, y + s * 5 / 8, x + s / 3, y + s * 3 / 4, s / 16 > 3 ? s / 16 : 4, BLACK);
      break;
    case 7:   /* agenda */
      FillArea(x + s / 6, y + s / 6, s * 2 / 3, s * 2 / 3, BLACK); FillArea(x + s / 6 + 4, y + s / 6 + s / 6, s * 2 / 3 - 8, s / 2 - 4, WHITE);
      for (i = 0; i < 2; i++) for (j = 0; j < 3; j++) FillArea(x + s / 4 + j * (s / 5), y + s * 7 / 16 + i * (s / 6), s / 10, s / 10, BLACK);
      break;
    case 8:   /* sudoku */
      for (i = 0; i <= 3; i++) { FillArea(x + s / 6 + i * (s * 2 / 9), y + s / 6, 4, s * 2 / 3 + 4, BLACK); FillArea(x + s / 6, y + s / 6 + i * (s * 2 / 9), s * 2 / 3 + 4, 4, BLACK); }
      FillArea(x + s / 6 + s / 14, y + s / 6 + s / 14, s / 12, s / 12, BLACK); FillArea(x + s / 6 + s * 2 / 9 + s / 14, y + s / 6 + s * 2 / 9 + s / 14, s / 12, s / 12, BLACK); FillArea(x + s / 6 + s * 4 / 9 + s / 14, y + s / 6 + s / 14, s / 12, s / 12, BLACK);
      break;
    case 9:   /* buscamines */
      thick(x + s / 2, y + s / 6, x + s / 2, y + s * 5 / 6, s / 16 > 3 ? s / 16 : 4, BLACK); thick(x + s / 6, y + s / 2, x + s * 5 / 6, y + s / 2, s / 16 > 3 ? s / 16 : 4, BLACK);
      thick(x + s / 4, y + s / 4, x + s * 3 / 4, y + s * 3 / 4, s / 16 > 3 ? s / 16 : 4, BLACK); thick(x + s * 3 / 4, y + s / 4, x + s / 4, y + s * 3 / 4, s / 16 > 3 ? s / 16 : 4, BLACK);
      disc(x + s / 2, y + s / 2, s / 4, BLACK); disc(x + s * 7 / 16, y + s * 7 / 16, s / 20, WHITE);
      break;
    case 10:  /* el temps: sol */
      thick(x + s / 2, y + s / 8, x + s / 2, y + s * 7 / 8, s / 20 > 2 ? s / 20 : 3, BLACK); thick(x + s / 8, y + s / 2, x + s * 7 / 8, y + s / 2, s / 20 > 2 ? s / 20 : 3, BLACK);
      thick(x + s / 4, y + s / 4, x + s * 3 / 4, y + s * 3 / 4, s / 20 > 2 ? s / 20 : 3, BLACK); thick(x + s * 3 / 4, y + s / 4, x + s / 4, y + s * 3 / 4, s / 20 > 2 ? s / 20 : 3, BLACK);
      disc(x + s / 2, y + s / 2, s / 4 + s / 20, WHITE); disc(x + s / 2, y + s / 2, s / 5, BLACK);
      break;
    case 11:  /* notícies */
      FillArea(x + s / 6, y + s / 6, s * 2 / 3, s * 2 / 3, BLACK); FillArea(x + s / 6 + 4, y + s / 6 + 4, s * 2 / 3 - 8, s * 2 / 3 - 8, WHITE);
      FillArea(x + s / 4, y + s / 4, s / 2, s / 8, BLACK);
      for (i = 0; i < 3; i++) FillArea(x + s / 4, y + s * 7 / 16 + i * (s / 9), s / 2, s / 32 > 1 ? s / 32 : 2, BLACK);
      break;
    case 12:  /* actualitza: fletxa cap avall sobre una safata */
      thick(x + s / 2, y + s / 6, x + s / 2, y + s * 5 / 8, s / 12 > 4 ? s / 12 : 5, BLACK);
      thick(x + s / 2, y + s * 5 / 8, x + s / 3, y + s * 7 / 16, s / 12 > 4 ? s / 12 : 5, BLACK); thick(x + s / 2, y + s * 5 / 8, x + s * 2 / 3, y + s * 7 / 16, s / 12 > 4 ? s / 12 : 5, BLACK);
      FillArea(x + s / 6, y + s * 3 / 4, s * 2 / 3, s / 12 > 4 ? s / 12 : 5, BLACK);
      break;
    default:  /* app instal·lada: quadrat negre (la lletra la posa qui crida) */
      FillArea(x + s / 6, y + s / 6, s * 2 / 3, s * 2 / 3, BLACK);
      break;
  }
}
#define NAPPS 13
static const char *APPS[NAPPS] = { "Bus AMB", "Rellotge", "Notes", "Calculadora", "Fitxers", "Tasques", "Conversor", "Agenda", "Sudoku", "Buscamines", "El temps", "Notícies", "Actualitza" };
static void ext_scan(void);
static char extApps[40][200]; static int nExt;
static void v_desk(View *v) {
  time_t now = time(NULL); struct tm *tm = localtime(&now); char t[16], d[200];
  int y = SB + U / 2, cols = 4, cw = (SW - U) / cols, s = cw * 55 / 100, cellh, per, i, total, k = 0;
  ext_scan();
  total = NAPPS + nExt;
  snprintf(t, sizeof(t), "%d:%02d", tm->tm_hour, tm->tm_min);
  SetFont(fHuge, BLACK); DrawTextRect(U / 2, y, SW - U, 4 * U, t, ALIGN_CENTER | VALIGN_MIDDLE); y += 4 * U;
  snprintf(d, sizeof(d), "%s, %d %s", WDAY[tm->tm_wday], tm->tm_mday, MONTH[tm->tm_mon]);
  SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, d, ALIGN_CENTER | VALIGN_MIDDLE); y += 2 * U + U / 2;
  FillArea(U / 2, y - U / 2, SW - U, 2, BLACK);
  cellh = s + 2 * U + U / 4;
  per = ((BOTTOM - y) / cellh) * cols; if (per < cols) per = cols;
  pages = (total + per - 1) / per; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < total && k < per; i++, k++) {
    int cx = U / 2 + (k % cols) * cw, cy = y + (k / cols) * cellh;
    if (i < NAPPS) { app_icon(i, cx + (cw - s) / 2, cy, s); snprintf(d, sizeof(d), "%s", APPS[i]); }
    else {
      size_t n;
      app_icon(-1, cx + (cw - s) / 2, cy, s);
      snprintf(d, sizeof(d), "%s", extApps[i - NAPPS]); n = strlen(d); if (n > 4) d[n - 4] = 0;
      t[0] = d[0]; t[1] = 0; if ((unsigned char)d[0] >= 0xC0) { t[1] = d[1]; t[2] = 0; }
      SetFont(fHuge, WHITE); DrawTextRect(cx + (cw - s) / 2, cy, s, s, t, ALIGN_CENTER | VALIGN_MIDDLE);
    }
    SetFont(fSmall, BLACK); DrawTextRect(cx + 2, cy + s, cw - 4, U + U / 2, d, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    add_hit(cx, cy, cw, s + 2 * U, A_APP, i);
  }
}
static void v_clock(View *v) {
  time_t now = time(NULL); struct tm tm = *localtime(&now), first; char t[16], d[100];
  int y = TOP, i, cw = (SW - U) / 7, ch = 2 * U, dim, start;
  static const char *WD[7] = { "dl", "dt", "dc", "dj", "dv", "ds", "dg" };
  static const int MD[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  (void)v;
  snprintf(d, sizeof(d), "%s, %d %s de %d", WDAY[tm.tm_wday], tm.tm_mday, MONTH[tm.tm_mon], tm.tm_year + 1900);
  header("Rellotge", d);
  snprintf(t, sizeof(t), "%d:%02d", tm.tm_hour, tm.tm_min);
  SetFont(fHuge, BLACK); DrawTextRect(U / 2, y, SW - U, 5 * U, t, ALIGN_CENTER | VALIGN_MIDDLE); y += 6 * U;
  snprintf(d, sizeof(d), "%s de %d", MONTHN[tm.tm_mon], tm.tm_year + 1900);
  SetFont(fBold, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, d, ALIGN_CENTER | VALIGN_MIDDLE); y += 2 * U + U / 2;
  SetFont(fSmall, BLACK);
  for (i = 0; i < 7; i++) DrawTextRect(U / 2 + i * cw, y, cw, U + U / 2, WD[i], ALIGN_CENTER | VALIGN_MIDDLE);
  y += U + U / 2; FillArea(U / 2, y, 7 * cw, 2, BLACK); y += U / 4;
  first = tm; first.tm_mday = 1; first.tm_isdst = -1; mktime(&first);
  start = (first.tm_wday + 6) % 7;
  dim = MD[tm.tm_mon]; if (tm.tm_mon == 1 && (((tm.tm_year + 1900) % 4 == 0 && (tm.tm_year + 1900) % 100 != 0) || (tm.tm_year + 1900) % 400 == 0)) dim = 29;
  for (i = 1; i <= dim; i++) {
    int pos = start + i - 1, x = U / 2 + (pos % 7) * cw, yy = y + (pos / 7) * ch;
    snprintf(t, sizeof(t), "%d", i);
    if (i == tm.tm_mday) { FillArea(x + cw / 8, yy + ch / 8, cw - cw / 4, ch - ch / 4, BLACK); SetFont(fBold, WHITE); } else SetFont(fText, BLACK);
    DrawTextRect(x, yy, cw, ch, t, ALIGN_CENTER | VALIGN_MIDDLE);
  }
}

/* ── notes: una per línia a un fitxer de text ── */
#define NOTESFILE FLASHDIR "/system/config/escriptori_notes.txt"
#define MAXNOTES 200
#define NOTELEN 400
static char (*notes)[NOTELEN]; static int nnotes, notesLoaded, noteEdit = -1;
static char nbuf[NOTELEN];
static void notes_load(void) {
  FILE *f;
  if (notesLoaded) return;
  notesLoaded = 1; nnotes = 0;
  if (!notes) notes = (char (*)[NOTELEN])calloc(MAXNOTES, NOTELEN);
  if (!notes) return;
  f = fopen(NOTESFILE, "r");
  if (!f) return;
  while (nnotes < MAXNOTES && fgets(notes[nnotes], NOTELEN, f)) {
    char *nl = strpbrk(notes[nnotes], "\r\n");
    if (nl) *nl = 0;
    if (notes[nnotes][0]) nnotes++;
  }
  fclose(f);
}
static void notes_save(void) {
  FILE *f = fopen(NOTESFILE, "w"); int i;
  if (!f) return;
  for (i = 0; i < nnotes; i++) fprintf(f, "%s\n", notes[i]);
  fclose(f);
}
static void v_notes(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 2, per, i; char sub[40];
  notes_load();
  snprintf(sub, sizeof(sub), nnotes == 1 ? "%d nota" : "%d notes", nnotes);
  header("Notes", sub);
  button(U / 2, y, SW - U, bh, "Nova nota", 0, A_NOTE_NEW, 0); y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = nnotes ? (nnotes + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!nnotes) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Encara no hi ha cap nota.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < nnotes && i < (v->page + 1) * per; i++) {
    box(U / 2, y, SW - U, rh - U / 4);
    SetFont(fText, BLACK); DrawTextRect(U, y, SW - 2 * U, rh - U / 4, notes[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_NOTE_OPEN, i);
    y += rh;
  }
}
static void v_note(View *v) {
  int ah = 2 * U + U / 4, bw = (SW - U - U / 3) / 2, y = BOTTOM - ah;
  notes_load();
  if (v->a < 0 || v->a >= nnotes) { header("Nota", ""); return; }
  header("Nota", "");
  SetFont(fText, BLACK);
  DrawTextRect(U / 2, TOP, SW - U, y - TOP - U, notes[v->a], ALIGN_LEFT | VALIGN_TOP);
  button(U / 2, y, bw, ah, "Edita", 0, A_NOTE_EDIT, v->a);
  button(U / 2 + bw + U / 3, y, bw, ah, "Esborra", 0, A_NOTE_DEL, v->a);
}
static void on_note_kbd(char *text) {
  if (text && notes) {
    char *s = text, *p; size_t n;
    while (*s == ' ') s++;
    for (p = s; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
    n = strlen(s); while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
    if (n > 0) {
      if (noteEdit >= 0 && noteEdit < nnotes) snprintf(notes[noteEdit], NOTELEN, "%s", s);
      else if (nnotes < MAXNOTES) { memmove(notes[1], notes[0], (size_t)nnotes * NOTELEN); snprintf(notes[0], NOTELEN, "%s", s); nnotes++; }
      notes_save();
    }
  }
  draw(1);
}
static int noteDel = -1;
static void on_note_del(int button) {
  if (button == 1 && noteDel >= 0 && noteDel < nnotes) {
    memmove(notes[noteDel], notes[noteDel + 1], (size_t)(nnotes - noteDel - 1) * NOTELEN);
    nnotes--; notes_save();
    if (depth > 1) depth--;
  }
  noteDel = -1;
  draw(1);
}

/* ── calculadora ── */
static const char *CKEYS[20] = { "C", "DEL", "%", "\xC3\xB7", "7", "8", "9", "\xC3\x97", "4", "5", "6", "-", "1", "2", "3", "+", "\xC2\xB1", "0", ",", "=" };
static char cdisp[40] = "0"; static double cacc; static int cop = -1, cfresh = 1;
static double cval(void) { char t[40]; int i; for (i = 0; cdisp[i] && i < 39; i++) t[i] = cdisp[i] == ',' ? '.' : cdisp[i]; t[i] = 0; return atof(t); }
static void cset(double v) {
  char *p;
  if (v != v || v > 1e15 || v < -1e15) { snprintf(cdisp, sizeof(cdisp), "Error"); return; }
  snprintf(cdisp, sizeof(cdisp), "%.10g", v);
  for (p = cdisp; *p; p++) if (*p == '.') *p = ',';
}
static int capply(void) {
  double b = cval(), r = b;
  if (cop == 3) { if (b == 0) { snprintf(cdisp, sizeof(cdisp), "Error"); cop = -1; cfresh = 1; return 0; } r = cacc / b; }
  else if (cop == 7) r = cacc * b; else if (cop == 11) r = cacc - b; else if (cop == 15) r = cacc + b;
  cset(r); return 1;
}
static void calc_key(int k) {
  size_t n = strlen(cdisp);
  if (!strcmp(cdisp, "Error") && k != 0) { snprintf(cdisp, sizeof(cdisp), "0"); cfresh = 1; cop = -1; n = 1; }
  if (k == 0) { snprintf(cdisp, sizeof(cdisp), "0"); cacc = 0; cop = -1; cfresh = 1; }
  else if (k == 1) { if (!cfresh) { if (n > 1) cdisp[n - 1] = 0; else snprintf(cdisp, sizeof(cdisp), "0"); if (!strcmp(cdisp, "-")) snprintf(cdisp, sizeof(cdisp), "0"); } }
  else if (k == 2) { cset(cval() / 100.0); cfresh = 1; }
  else if (k == 16) { if (cdisp[0] == '-') memmove(cdisp, cdisp + 1, n); else if (strcmp(cdisp, "0") && n < 38) { memmove(cdisp + 1, cdisp, n + 1); cdisp[0] = '-'; } }
  else if (k == 18) { if (cfresh) { snprintf(cdisp, sizeof(cdisp), "0,"); cfresh = 0; } else if (!strchr(cdisp, ',') && n < 14) { cdisp[n] = ','; cdisp[n + 1] = 0; } }
  else if (k == 19) { if (cop >= 0) { capply(); cop = -1; } cfresh = 1; }
  else if (k == 3 || k == 7 || k == 11 || k == 15) { if (cop >= 0 && !cfresh) { if (!capply()) return; } cacc = cval(); cop = k; cfresh = 1; }
  else {                                                 /* dígit */
    const char *dg = CKEYS[k];
    if (cfresh || !strcmp(cdisp, "0")) { snprintf(cdisp, sizeof(cdisp), "%s", dg); cfresh = 0; }
    else if (n < 14) { cdisp[n] = dg[0]; cdisp[n + 1] = 0; }
  }
}
static void calc_display(void) {
  int dh = 4 * U;
  FillArea(U / 2, TOP, SW - U, dh, WHITE); box(U / 2, TOP, SW - U, dh);
  SetFont(fHuge, BLACK); DrawTextRect(U, TOP, SW - 2 * U, dh, cdisp, ALIGN_RIGHT | VALIGN_MIDDLE);
  if (cop >= 0) { SetFont(fBold, BLACK); DrawTextRect(U, TOP + U / 4, 2 * U, U + U / 2, CKEYS[cop], ALIGN_LEFT | VALIGN_MIDDLE); }
}
static void v_calc(View *v) {
  int dh = 4 * U, y0 = TOP + dh + U / 2, gap = U / 4, bw = (SW - U - 3 * gap) / 4, bh = (BOTTOM + NAVH - y0 - 4 * gap) / 5, i;
  (void)v;
  header("Calculadora", "");
  calc_display();
  if (bh > 4 * U) bh = 4 * U;
  for (i = 0; i < 20; i++) button(U / 2 + (i % 4) * (bw + gap), y0 + (i / 4) * (bh + gap), bw, bh, CKEYS[i], i == 19, A_CALC, i);
}

static const char *pendMsg;
static void say(const char *m) { pendMsg = m; }
#include "suite.c"

static void draw(int full) {
  View *v = &stack[depth - 1];
  int pass;
  /* dues passades si la pàgina demanada queda fora de rang després de calcular-la */
  for (pass = 0; pass < 2; pass++) {
    int before = v->page;
    ClearScreen(); nhits = 0; pages = 1;
    statusbar(1);
    if (loadErr && v->type < V_DESK) {
      header("Bus AMB", "Error");
      SetFont(fText, BLACK); DrawTextRect(U / 2, TOP, SW - U, 4 * U, "No s'han pogut carregar les dades de l'aplicació.", ALIGN_LEFT | VALIGN_TOP);
      break;
    }
    switch (v->type) {
      case V_HOME: v_home(v); break;
      case V_SEARCH: v_search(v); break;
      case V_LINES: v_lines(v); break;
      case V_LINE: v_line(v); break;
      case V_LINESTOPS: v_linestops(v); break;
      case V_STOP: v_stop(v); break;
      case V_TTPICK: v_ttpick(v); break;
      case V_TT: v_tt(v); break;
      case V_DESK: v_desk(v); break;
      case V_CLOCK: v_clock(v); break;
      case V_NOTES: v_notes(v); break;
      case V_NOTE: v_note(v); break;
      case V_CALC: v_calc(v); break;
      case V_FILES: v_files(v); break;
      case V_FILE: v_file(v); break;
      case V_TODO: v_todo(v); break;
      case V_CONV: v_conv(v); break;
      case V_AGENDA: v_agenda(v); break;
      case V_DAY: v_day(v); break;
      case V_SUDOKU: v_sudoku(v); break;
      case V_MINES: v_mines(v); break;
      case V_WEATHER: v_weather(v); break;
      case V_RSS: v_rss(v); break;
      case V_RSSITEM: v_rssitem(v); break;
      case V_FEEDS: v_feeds(v); break;
      case V_UPDATE: v_update(v); break;
    }
    if (v->page < 0) v->page = 0;
    footer(v->page);
    if (v->page == before) break;
  }
  if (full) FullUpdate(); else SoftUpdate();
  if (pendMsg) { const char *m = pendMsg; pendMsg = NULL; Message(ICON_INFORMATION, "Escriptori", m, 4000); }
}
static void push(int type, int a, int b, int sel, const char *term) {
  View *v;
  if (depth >= MAXSTACK) { memmove(&stack[1], &stack[2], sizeof(View) * (MAXSTACK - 2)); depth--; }   /* la base (escriptori) es conserva */
  v = &stack[depth++];
  memset(v, 0, sizeof(*v));
  v->type = type; v->a = a; v->b = b; v->sel = sel;
  if (term) { strncpy(v->term, term, sizeof(v->term) - 1); }
  draw(1);
}
static void on_kbd(char *text) {
  if (text && text[0]) {
    char *s = text; while (*s == ' ') s++;
    if (*s) { push(V_SEARCH, 0, 0, 0, s); return; }
  }
  draw(1);
}
static void on_pdf_dialog(int button) {
  if (button == 1 && pdfPath[0]) OpenBook(pdfPath, "", 0);
  else draw(1);
}
static void save_pdf(View *v) {
  char slug[128], msg[700]; const char *lab = amb_route_label(ctt.route); int rc, i, n = 0;
  mkdir(PDFDIR, 0777);
  for (i = 0; lab[i] && n < 20; i++) if ((lab[i] >= '0' && lab[i] <= '9') || (lab[i] >= 'A' && lab[i] <= 'Z') || (lab[i] >= 'a' && lab[i] <= 'z')) slug[n++] = lab[i];
  slug[n] = 0;
  snprintf(pdfPath, sizeof(pdfPath), PDFDIR "/horari-%s-%s-%d.pdf", db.sCode[v->a][0] ? db.sCode[v->a] : "parada", slug[0] ? slug : "linia", v->b + 1);
  ShowHourglass();
  rc = amb_write_pdf(pdfPath, &ctt, time(NULL));
  HideHourglass();
  if (rc == 0) {
    snprintf(msg, sizeof(msg), "S'ha desat a la carpeta «Horaris AMB»:\n%s", strrchr(pdfPath, '/') + 1);
    Dialog(ICON_INFORMATION, "PDF desat", msg, "Obre'l", "Tanca", on_pdf_dialog);
  } else {
    pdfPath[0] = 0;
    Message(ICON_ERROR, "Error", "No s'ha pogut desar el PDF.", 4000);
  }
}
static void act(int a, int arg) {
  View *v = &stack[depth - 1];
  switch (a) {
    case A_BACK:
      if (v->type == V_FILES && strlen(fpath) > strlen(FLASHDIR)) { files_up(); v->page = 0; draw(1); }
      else if (depth > 1) { depth--; draw(1); }
      break;
    case A_HOME: depth = 1; stack[0].page = 0; draw(1); break;
    case A_PREV: if (v->page > 0) { v->page--; draw(0); } break;
    case A_NEXT: if (v->page < pages - 1) { v->page++; draw(0); } break;
    case A_SEARCH: kbuf[0] = 0; OpenKeyboard("Nom o codi de la parada", kbuf, 60, KBD_NORMAL, on_kbd); break;
    case A_LINES: push(V_LINES, 0, 0, 0, NULL); break;
    case A_LINE: push(V_LINE, arg, 0, 0, NULL); break;
    case A_PATTERN: push(V_LINESTOPS, v->a, arg, 0, NULL); break;
    case A_STOP: push(V_STOP, arg, 0, -1, NULL); break;
    case A_FILTER: v->sel = arg; v->page = 0; draw(0); break;
    case A_REFRESH: v->page = 0; draw(1); break;
    case A_FAV: { int i = fav_index(arg); if (i >= 0) { memmove(&favs[i], &favs[i + 1], sizeof(int) * (size_t)(nfavs - i - 1)); nfavs--; } else if (favs && nfavs < 64) favs[nfavs++] = arg; favs_save(); draw(0); } break;
    case A_TTPICK: push(V_TTPICK, arg, 0, 0, NULL); break;
    case A_TT: push(V_TT, v->a, arg, 0, NULL); break;
    case A_TAB: v->sel = arg; v->page = 0; draw(0); break;
    case A_PDF: save_pdf(v); break;
    case A_EXIT: CloseApp(); break;
    case A_APP: {
      static const int VT[NAPPS] = { V_HOME, V_CLOCK, V_NOTES, V_CALC, V_FILES, V_TODO, V_CONV, V_AGENDA, V_SUDOKU, V_MINES, V_WEATHER, V_RSS, V_UPDATE };
      if (arg >= NAPPS) { ext_launch(arg - NAPPS); break; }
      if (arg < 0) break;
      if (VT[arg] == V_FILES) { snprintf(fpath, sizeof(fpath), "%s", FLASHDIR); }
      if (VT[arg] == V_AGENDA) { time_t now = time(NULL); struct tm *tm = localtime(&now); push(V_AGENDA, (tm->tm_year + 1900) * 12 + tm->tm_mon, 0, 0, NULL); break; }
      push(VT[arg], 0, 0, VT[arg] == V_HOME ? 0 : 0, NULL);
    } break;
    case A_FOPEN:
      if (arg < 0 || arg >= nfents) break;
      if (fents[arg].dir) { if (strlen(fpath) + strlen(fents[arg].name) + 2 < sizeof(fpath)) { strcat(fpath, "/"); strcat(fpath, fents[arg].name); v->page = 0; } draw(1); }
      else { snprintf(fsel, sizeof(fsel), "%s/%s", fpath, fents[arg].name); push(V_FILE, 0, 0, 0, NULL); }
      break;
    case A_FACT:
      if (arg == 0) { if (!OpenBook(fsel, "", 0)) { say("No s'ha pogut obrir aquest fitxer."); draw(0); } }
      else if (arg == 1) confirm("Esborra el fitxer", "S'esborrarà definitivament. Vols continuar?", "Esborra", file_delete);
      else { snprintf(moveSrc, sizeof(moveSrc), "%s", fsel); if (depth > 1) depth--; draw(1); }
      break;
    case A_FMOVEHERE: {
      char dest[1000]; struct stat st2; const char *nm = strrchr(moveSrc, '/');
      snprintf(dest, sizeof(dest), "%s/%s", fpath, nm ? nm + 1 : moveSrc);
      if (!strcmp(dest, moveSrc)) say("El fitxer ja és en aquesta carpeta.");
      else if (stat(dest, &st2) == 0) say("Ja hi ha un fitxer amb aquest nom en aquesta carpeta.");
      else if (rename(moveSrc, dest) != 0) say("No s'ha pogut moure el fitxer.");
      moveSrc[0] = 0; draw(1);
    } break;
    case A_FMOVECANCEL: moveSrc[0] = 0; draw(0); break;
    case A_TODO_NEW: ask("Nova tasca", "", todo_add); break;
    case A_TODO_TOGGLE: if (arg >= 0 && arg < todo.n) { todo.it[arg][0] = todo.it[arg][0] == '1' ? '0' : '1'; st_save(&todo); draw(0); } break;
    case A_TODO_CLEAR: confirm("Tasques", "Vols esborrar les tasques fetes?", "Esborra", todo_clear); break;
    case A_CONV_CAT: cvCat = arg; cvFrom = 0; draw(0); break;
    case A_CONV_UNIT: cvFrom = arg; draw(0); break;
    case A_CONV_VAL: { char num[48]; fmt_num(num, sizeof(num), cvVal); ask("Valor", num, conv_set); } break;
    case A_AG_MONTH: v->a += arg; draw(0); break;
    case A_AG_DAY: push(V_DAY, arg, 0, 0, NULL); break;
    case A_AG_NEW: agDay = arg; ask("Nou esdeveniment", "", ag_add); break;
    case A_AG_DEL: agDel = arg; confirm("Agenda", "Vols esborrar aquest esdeveniment?", "Esborra", ag_del); break;
    case A_SU_CELL: suSel = suSel == arg ? -1 : arg; draw(0); break;
    case A_SU_NUM: if (suSel >= 0 && !suG[suSel]) { suC[suSel] = (char)arg; su_save(); draw(0); } break;
    case A_SU_NEW: confirm("Sudoku nou", "Es perdrà la partida actual.", "Sudoku nou", su_new); break;
    case A_SU_CHECK: {
      int i, bad = 0, left = 0; static char msg[100];
      for (i = 0; i < 81; i++) { if (!suC[i]) left++; else if (suC[i] != suSol[i]) bad++; }
      if (bad) snprintf(msg, sizeof(msg), bad == 1 ? "Hi ha %d casella equivocada." : "Hi ha %d caselles equivocades.", bad);
      else if (left) snprintf(msg, sizeof(msg), "Tot correcte fins ara. Falten %d caselles.", left);
      else snprintf(msg, sizeof(msg), "Sudoku resolt. Enhorabona!");
      say(msg); draw(0);
    } break;
    case A_MI_CELL:
      if (miState != 0 || arg < 0 || arg >= MN * MN) break;
      if (miFlagMode) { if (miSt[arg] == 0) miSt[arg] = 2; else if (miSt[arg] == 2) miSt[arg] = 0; }
      else if (miSt[arg] == 0) mi_reveal(arg);
      draw(0);
      break;
    case A_MI_MODE: miFlagMode = !miFlagMode; draw(0); break;
    case A_MI_NEW: mi_new(); draw(1); break;
    case A_WX_REFRESH: wx_refresh(); draw(1); break;
    case A_RSS_REFRESH: rss_refresh(); v->page = 0; draw(1); break;
    case A_RSS_OPEN: push(V_RSSITEM, arg, 0, 0, NULL); break;
    case A_RSS_FEEDS: push(V_FEEDS, 0, 0, 0, NULL); break;
    case A_FEED_ADD: ask("Adreça del canal RSS", "https://", feed_add); break;
    case A_UPD_CHECK: upd_check(); draw(1); break;
    case A_UPD_INSTALL: upd_install(); draw(1); break;
    case A_FEED_DEL: feedDel = arg; confirm("Canals", "Vols esborrar aquest canal?", "Esborra", feed_del); break;
    case A_NOTE_NEW: noteEdit = -1; nbuf[0] = 0; OpenKeyboard("Nova nota", nbuf, NOTELEN - 20, KBD_NORMAL, on_note_kbd); break;
    case A_NOTE_OPEN: push(V_NOTE, arg, 0, 0, NULL); break;
    case A_NOTE_EDIT: if (arg >= 0 && arg < nnotes) { noteEdit = arg; snprintf(nbuf, sizeof(nbuf), "%s", notes[arg]); OpenKeyboard("Edita la nota", nbuf, NOTELEN - 20, KBD_NORMAL, on_note_kbd); } break;
    case A_NOTE_DEL: noteDel = arg; Dialog(ICON_QUESTION, "Esborra la nota", "Vols esborrar aquesta nota?", "Esborra", "Cancel·la", on_note_del); break;
    case A_CALC: calc_key(arg); calc_display(); PartialUpdate(U / 2, TOP, SW - U, 4 * U); break;
  }
}

static int handler(int type, int par1, int par2) {
  int i;
  switch (type) {
    case EVT_INIT:
      SW = ScreenWidth(); SH = ScreenHeight(); U = SW / 27; if (U < 16) U = 16; SB = U * 9 / 4;
      SetPanelType(PANEL_DISABLED);
      SH -= PanelHeight();
      fTitle = OpenFont(DEFAULTFONTB, U * 11 / 10, 1); fBold = OpenFont(DEFAULTFONTB, U * 9 / 10, 1);
      fText = OpenFont(DEFAULTFONT, U * 9 / 10, 1); fSmall = OpenFont(DEFAULTFONT, U * 7 / 10, 1); fBig = OpenFont(DEFAULTFONTB, U * 12 / 10, 1); fHuge = OpenFont(DEFAULTFONTB, U * 3, 1);
      loadErr = amb_load(amb_blob, (long)(amb_blob_end - amb_blob));
      if (!loadErr) favs_load();
      depth = 1; memset(&stack[0], 0, sizeof(View)); stack[0].type = V_DESK;
      break;
    case EVT_SHOW:
      draw(1);
      ClearTimer(clock_tick);
      SetWeakTimer("busamb_clock", clock_tick, (int)(61 - time(NULL) % 60) * 1000);
      break;
    case EVT_BACKGROUND:
      ClearTimer(clock_tick);
      break;
    case EVT_FOREGROUND:
      draw(1);
      ClearTimer(clock_tick);
      SetWeakTimer("busamb_clock", clock_tick, (int)(61 - time(NULL) % 60) * 1000);
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
