/* Escriptori — components del sistema de disseny que necessiten tipografies i tacte.
   S'inclou des de mod.h (paquets) i des de base.c (sistema base), un cop existeixen SW, U, fonts i add_hit. */
#ifndef ESC_UI_H
#define ESC_UI_H

/* Fila de llista dins una targeta: títol, subtítol opcional i text a la dreta (estat). Tota la fila és tàctil. */
static UNUSED void ui_row(int x, int y, int w, int h, const char *title, const char *sub, const char *right, int act, int arg) {
  int rw = 0, pad = U / 2;
  ui_card(x, y, w, h, UI_LINE);
  if (right && right[0]) { SetFont(fSmall, DGRAY); rw = StringWidth(right) + U / 2; DrawTextRect(x + w - pad - rw, y, rw, sub ? h / 2 + U / 4 : h, right, ALIGN_RIGHT | VALIGN_MIDDLE); }
  SetFont(fBold, BLACK);
  DrawTextRect(x + pad, y + (sub ? U / 8 : 0), w - 2 * pad - rw, sub ? h / 2 : h, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  if (sub) { SetFont(fSmall, DGRAY); DrawTextRect(x + pad, y + h / 2 - U / 8, w - 2 * pad, h / 2 - U / 8, sub, ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  if (act) add_hit(x, y, w, h, act, arg);
}
/* Etiqueta petita sobre un valor gran: el format estàndard d'un widget d'una línia (alçada 1). */
static UNUSED void ui_lv(int x, int y, int w, int h, const char *label, const char *value) {
  SetFont(fSmall, DGRAY); DrawTextRect(x, y, w, h * 4 / 10, label, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fBig, BLACK); DrawTextRect(x, y + h * 4 / 10, w, h * 6 / 10, value, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
}
/* Interruptor: només el dibuix (el tacte el gestiona qui l'utilitza). */
static UNUSED void ui_toggle(int x, int y, int w, int h, int on) {
  int r = h / 2, k = h - 8;
  if (on) { rfill(x, y, w, h, r, BLACK); rfill(x + w - k - 4, y + 4, k, k, k / 2, WHITE); }
  else { rcard(x, y, w, h, r, 2, BLACK, WHITE); rfill(x + 4, y + 4, k, k, k / 2, BLACK); }
}
/* Rajola del tauler ràpid: targeta amb títol i estat; solid = invertida (activa). L'icona la dibuixa qui la crida. */
static UNUSED void ui_tile(int x, int y, int w, int h, const char *title, const char *state, int solid, int act, int arg) {
  int fg = solid ? WHITE : BLACK;
  ui_card(x, y, w, h, solid ? UI_SOLID : UI_LINE);
  SetFont(fBold, fg); DrawTextRect(x + U / 2, y + h - 2 * U - U / 4, w - U, U + U / 4, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fSmall, solid ? WHITE : DGRAY); DrawTextRect(x + U / 2, y + h - U - U / 4, w - U, U, state ? state : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  if (act) add_hit(x, y, w, h, act, arg);
}
#endif
