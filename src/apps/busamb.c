/* Escriptori — paquet «Bus AMB». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_HOME, V_SEARCH, V_LINES, V_LINE, V_LINESTOPS, V_STOP, V_TTPICK, V_TT };
enum { A_SEARCH = 100, A_LINES, A_LINE, A_PATTERN, A_STOP, A_FILTER, A_REFRESH, A_FAV, A_TTPICK, A_TT, A_TAB, A_PDF };
#include "busamb/amb.h"
extern const unsigned char amb_blob[], amb_blob_end[];
static int loadErr, loaded;
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
static void on_kbd(char *text) {
  if (text && text[0]) {
    char *s = text; while (*s == ' ') s++;
    if (*s) { push(V_SEARCH, 0, 0, 0, s); return; }
  }
  draw(1);
}
static void on_pdf_dialog(int btn) {
  if (btn == 1 && pdfPath[0]) OpenBook(pdfPath, "", 0);
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
static void mod_icon(int x, int y, int s) {      /* icona de línia, mateix gruix que la resta (ic_t) */
  int t = ic_t(s);
  ol(x + s / 5, y + s / 6, s * 3 / 5, s * 7 / 12, s / 10, t);
  FillArea(x + s / 5, y + s * 7 / 16, s * 3 / 5, t, BLACK);
  disc(x + s * 11 / 32, y + s * 19 / 32, t * 2 / 3 + 1, BLACK); disc(x + s * 21 / 32, y + s * 19 / 32, t * 2 / 3 + 1, BLACK);
  FillArea(x + s * 5 / 16 - t / 2, y + s * 3 / 4, t, s / 10, BLACK); FillArea(x + s * 11 / 16 - t / 2, y + s * 3 / 4, t, s / 10, BLACK);
}
static void mod_draw(View *v) {
  if (loadErr) { header("Bus AMB", "Error"); SetFont(fText, BLACK); DrawTextRect(U / 2, TOP, SW - U, 4 * U, "No s'han pogut carregar les dades d'horaris.", ALIGN_LEFT | VALIGN_TOP); return; }
  switch (v->type) {
    case V_HOME: v_home(v); break;
    case V_SEARCH: v_search(v); break;
    case V_LINES: v_lines(v); break;
    case V_LINE: v_line(v); break;
    case V_LINESTOPS: v_linestops(v); break;
    case V_STOP: v_stop(v); break;
    case V_TTPICK: v_ttpick(v); break;
    case V_TT: v_tt(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
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
  }
}
static void mod_open(View *v) {
  (void)v;
  if (!loaded) { loaded = 1; loadErr = amb_load(amb_blob, (long)(amb_blob_end - amb_blob)); if (!loadErr) favs_load(); }
}
static Module M = { ESC_API_MIN, "busamb", "Bus AMB", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
