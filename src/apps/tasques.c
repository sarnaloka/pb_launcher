/* Escriptori — paquet «Tasques». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_TODO };
enum { A_TODO_NEW = 100, A_TODO_TOGGLE, A_TODO_CLEAR };
/* ═════════════ Tasques ═════════════ */
static Store todo = { 0, 0, 0, CFG "escriptori_tasques.txt" };
static void todo_add(const char *t) { char ln[NOTELEN + 8]; snprintf(ln, sizeof(ln), "0\t%s", t); st_insert(&todo, 0, ln); st_save(&todo); }
static void todo_clear(void) { int i; for (i = todo.n - 1; i >= 0; i--) if (todo.it[i][0] == '1') st_del(&todo, i); st_save(&todo); }
static void v_todo(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 4, per, i, done = 0, bw = (SW - U - U / 3) / 2; char sub[64];
  st_load(&todo);
  for (i = 0; i < todo.n; i++) if (todo.it[i][0] == '1') done++;
  snprintf(sub, sizeof(sub), "%d pendents · %d fetes", todo.n - done, done);
  header("Tasques", sub);
  button(U / 2, y, bw, bh, "Nova tasca", 0, A_TODO_NEW, 0);
  if (done) button(U / 2 + bw + U / 3, y, bw, bh, "Esborra les fetes", 0, A_TODO_CLEAR, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = todo.n ? (todo.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!todo.n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "No hi ha cap tasca.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < todo.n && i < (v->page + 1) * per; i++) {
    int isDone = todo.it[i][0] == '1', cs = U + U / 4, cy = y + (rh - U / 4 - cs) / 2; const char *txt = strchr(todo.it[i], '\t');
    txt = txt ? txt + 1 : todo.it[i];
    box(U, cy, cs, cs);
    if (isDone) { thick(U + cs / 5, cy + cs / 2, U + cs * 2 / 5, cy + cs * 3 / 4, 4, BLACK); thick(U + cs * 2 / 5, cy + cs * 3 / 4, U + cs * 4 / 5, cy + cs / 5, 4, BLACK); }
    SetFont(fText, isDone ? DGRAY : BLACK);
    DrawTextRect(U + cs + U / 2, y, SW - 2 * U - cs - U / 2, rh - U / 4, txt, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LGRAY);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_TODO_TOGGLE, i);
    y += rh;
  }
}
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      for (i = 0; i < 3; i++) {
        int yy = y + s / 5 + i * (s / 4);
        FillArea(x + s / 6, yy, s / 6, s / 6, BLACK); if (i > 0) FillArea(x + s / 6 + 3, yy + 3, s / 6 - 6, s / 6 - 6, WHITE);
        FillArea(x + s / 6 + s / 4, yy + s / 16, s / 2 - s / 12, s / 20 > 2 ? s / 20 : 3, BLACK);
      }
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_TODO: v_todo(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_TODO_NEW: ask("Nova tasca", "", todo_add); break;
    case A_TODO_TOGGLE: if (arg >= 0 && arg < todo.n) { todo.it[arg][0] = todo.it[arg][0] == '1' ? '0' : '1'; st_save(&todo); draw(0); } break;
    case A_TODO_CLEAR: confirm("Tasques", "Vols esborrar les tasques fetes?", "Esborra", todo_clear); break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "tasques", "Tasques", 0, mod_icon, mod_draw, mod_action, NULL, mod_open };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
