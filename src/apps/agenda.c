/* Escriptori — paquet «Agenda». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_AGENDA, V_DAY };
enum { A_AG_MONTH = 100, A_AG_DAY, A_AG_NEW, A_AG_DEL };
/* ═════════════ Agenda ═════════════ */
static Store agenda = { 0, 0, 0, CFG "escriptori_agenda.txt" };   /* "AAAAMMDD\ttext", ordenat per data */
static int agDay;
static int ag_count(int date) { int i, n = 0; for (i = 0; i < agenda.n; i++) if (atoi(agenda.it[i]) == date) n++; return n; }
static void ag_add(const char *t) {
  char ln[NOTELEN + 16]; int i;
  snprintf(ln, sizeof(ln), "%08d\t%s", agDay, t);
  for (i = 0; i < agenda.n && atoi(agenda.it[i]) <= agDay; i++) {}
  st_insert(&agenda, i, ln); st_save(&agenda);
}
static int agDel = -1;
static void ag_del(void) { st_del(&agenda, agDel); st_save(&agenda); agDel = -1; }
static void v_agenda(View *v) {
  time_t now = time(NULL); struct tm tm = *localtime(&now);
  int y = TOP, yy = v->a / 12, mm = v->a % 12, today = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday, cw = (SW - U) / 7, ch = 2 * U, i, start, dim, bw = (SW - U - U / 3) / 2, bh = 2 * U, rows;
  static const char *WD[7] = { "dl", "dt", "dc", "dj", "dv", "ds", "dg" };
  char t[200];
  st_load(&agenda);
  snprintf(t, sizeof(t), "%s de %d", MONTHN[mm], yy);
  header("Agenda", t);
  button(U / 2, y, bw, bh, "< Mes anterior", 0, A_AG_MONTH, -1); button(U / 2 + bw + U / 3, y, bw, bh, "Mes següent >", 0, A_AG_MONTH, 1);
  y += bh + U / 2;
  SetFont(fSmall, BLACK);
  for (i = 0; i < 7; i++) DrawTextRect(U / 2 + i * cw, y, cw, U + U / 4, WD[i], ALIGN_CENTER | VALIGN_MIDDLE);
  y += U + U / 4; FillArea(U / 2, y, 7 * cw, 2, BLACK); y += U / 4;
  start = (weekday_of(yy, mm, 1) + 6) % 7; dim = days_in_month(yy, mm);
  for (i = 1; i <= dim; i++) {
    int pos = start + i - 1, x = U / 2 + (pos % 7) * cw, cy = y + (pos / 7) * ch, date = yy * 10000 + (mm + 1) * 100 + i, has = ag_count(date);
    snprintf(t, sizeof(t), "%d", i);
    if (date == today) { FillArea(x + cw / 8, cy + ch / 8, cw - cw / 4, ch - ch / 4, BLACK); SetFont(fBold, WHITE); } else SetFont(fText, BLACK);
    DrawTextRect(x, cy, cw, ch, t, ALIGN_CENTER | VALIGN_MIDDLE);
    if (has) FillArea(x + cw / 2 - U / 6, cy + ch - ch / 5, U / 3, U / 6, date == today ? WHITE : BLACK);
    add_hit(x, cy, cw, ch, A_AG_DAY, date);
  }
  rows = (start + dim + 6) / 7;
  y += rows * ch + U / 2;
  SetFont(fBold, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, "Propers esdeveniments", ALIGN_LEFT | VALIGN_MIDDLE); y += U + U * 3 / 4;
  {
    int shown = 0;
    for (i = 0; i < agenda.n && y + U + U / 2 <= BOTTOM + NAVH; i++) {
      int date = atoi(agenda.it[i]); const char *txt = strchr(agenda.it[i], '\t');
      if (date < today) continue;
      snprintf(t, sizeof(t), "%02d/%02d   %s", date % 100, date / 100 % 100, txt ? txt + 1 : "");
      SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      y += U + U * 3 / 4; shown++;
    }
    if (!shown) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, "Cap. Toca un dia per afegir-ne.", ALIGN_LEFT | VALIGN_MIDDLE); }
  }
}
static void v_day(View *v) {
  int date = v->a, yy = date / 10000, mm = date / 100 % 100 - 1, dd = date % 100, y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 4, per, i, n = 0, shown = 0;
  char title[100], sub[40];
  st_load(&agenda);
  n = ag_count(date);
  if (mm < 0 || mm > 11) mm = 0;
  snprintf(title, sizeof(title), "%s, %d %s", WDAY[weekday_of(yy, mm, dd)], dd, MONTH[mm]);
  snprintf(sub, sizeof(sub), n == 1 ? "%d esdeveniment" : "%d esdeveniments", n);
  header(title, sub);
  button(U / 2, y, SW - U, bh, "Nou esdeveniment", 0, A_AG_NEW, date); y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = n ? (n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = 0; i < agenda.n; i++) {
    const char *txt;
    if (atoi(agenda.it[i]) != date) continue;
    if (shown >= v->page * per && shown < (v->page + 1) * per) {
      txt = strchr(agenda.it[i], '\t');
      box(U / 2, y, SW - U, rh - U / 4);
      SetFont(fText, BLACK); DrawTextRect(U, y, SW - 2 * U, rh - U / 4, txt ? txt + 1 : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      add_hit(U / 2, y, SW - U, rh - U / 4, A_AG_DEL, i);
      y += rh;
    }
    shown++;
  }
  if (n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y + U / 4, SW - U, U + U / 4, "Toca un esdeveniment per esborrar-lo.", ALIGN_LEFT | VALIGN_MIDDLE); }
}
/* ───── widget ───── */
static void w_next(int x, int y, int w, int h) {
  time_t now = time(NULL); struct tm tm = *localtime(&now); int today = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday, i; char t[300] = "Cap esdeveniment proper";
  st_load(&agenda);
  for (i = 0; i < agenda.n; i++) {
    int date = atoi(agenda.it[i]); const char *txt = strchr(agenda.it[i], '\t');
    if (date < today) continue;
    snprintf(t, sizeof(t), "%02d/%02d   %s", date % 100, date / 100 % 100, txt ? txt + 1 : "");
    break;
  }
  ui_lv(x, y, w, h, "Agenda · proper esdeveniment", t);
}
static const Widget WG[] = { { "proper", "Proper esdeveniment", 4, 1, w_next } };
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      FillArea(x + s / 6, y + s / 6, s * 2 / 3, s * 2 / 3, BLACK); FillArea(x + s / 6 + 4, y + s / 6 + s / 6, s * 2 / 3 - 8, s / 2 - 4, WHITE);
      for (i = 0; i < 2; i++) for (j = 0; j < 3; j++) FillArea(x + s / 4 + j * (s / 5), y + s * 7 / 16 + i * (s / 6), s / 10, s / 10, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_AGENDA: v_agenda(v); break;
    case V_DAY: v_day(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_AG_MONTH: v->a += arg; draw(0); break;
    case A_AG_DAY: push(V_DAY, arg, 0, 0, NULL); break;
    case A_AG_NEW: agDay = arg; ask("Nou esdeveniment", "", ag_add); break;
    case A_AG_DEL: agDel = arg; confirm("Agenda", "Vols esborrar aquest esdeveniment?", "Esborra", ag_del); break;
  }
}
static void mod_open(View *v) {
  (void)v;
  { time_t now = time(NULL); struct tm *tm = localtime(&now); v->a = (tm->tm_year + 1900) * 12 + tm->tm_mon; }
}
static Module M = { ESC_API, "agenda", "Agenda", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 1, WG };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
