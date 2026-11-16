/* Escriptori — paquet «Rellotge». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_CLOCK };
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

static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      disc(x + s / 2, y + s / 2, s * 3 / 8, BLACK); disc(x + s / 2, y + s / 2, s * 3 / 8 - s / 16, WHITE);
      thick(x + s / 2, y + s / 2, x + s / 2, y + s / 4, s / 20 > 2 ? s / 20 : 3, BLACK);
      thick(x + s / 2, y + s / 2, x + s * 11 / 16, y + s * 9 / 16, s / 20 > 2 ? s / 20 : 3, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_CLOCK: v_clock(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "rellotge", "Rellotge", MOD_TICK, mod_icon, mod_draw, mod_action, NULL, mod_open };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
