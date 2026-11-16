/* Escriptori — paquet «Buscamines». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_MINES };
enum { A_MI_CELL = 100, A_MI_MODE, A_MI_NEW };
/* ═════════════ Buscamines ═════════════ */
#define MN 9
#define MINES 10
static char miMine[MN * MN], miSt[MN * MN]; static int miState, miStarted, miFlagMode, miInit;
static void mi_new(void) { memset(miMine, 0, sizeof(miMine)); memset(miSt, 0, sizeof(miSt)); miState = 0; miStarted = 0; miFlagMode = 0; miInit = 1; }
static int mi_adj(int i) {
  int r = i / MN, c = i % MN, dr, dc, n = 0;
  for (dr = -1; dr <= 1; dr++) for (dc = -1; dc <= 1; dc++) { int rr = r + dr, cc = c + dc; if ((dr || dc) && rr >= 0 && rr < MN && cc >= 0 && cc < MN && miMine[rr * MN + cc]) n++; }
  return n;
}
static void mi_reveal(int start) {
  int stack2[MN * MN * 9], sp = 0, k;
  if (!miStarted) {
    int placed = 0;
    while (placed < MINES) { int p = rand() % (MN * MN), dr = p / MN - start / MN, dc = p % MN - start % MN; if (miMine[p] || (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1)) continue; miMine[p] = 1; placed++; }
    miStarted = 1;
  }
  if (miSt[start] != 0) return;
  if (miMine[start]) { miState = 1; for (k = 0; k < MN * MN; k++) if (miMine[k]) miSt[k] = 1; return; }
  stack2[sp++] = start;
  while (sp > 0) {
    int i = stack2[--sp], r = i / MN, c = i % MN, dr, dc;
    if (miSt[i] != 0) continue;
    miSt[i] = 1;
    if (mi_adj(i)) continue;
    for (dr = -1; dr <= 1; dr++) for (dc = -1; dc <= 1; dc++) { int rr = r + dr, cc = c + dc; if ((dr || dc) && rr >= 0 && rr < MN && cc >= 0 && cc < MN && miSt[rr * MN + cc] == 0 && sp < MN * MN * 9) stack2[sp++] = rr * MN + cc; }
  }
  for (k = 0, sp = 0; k < MN * MN; k++) if (miSt[k] == 1) sp++;
  if (sp == MN * MN - MINES) miState = 2;
}
static void v_mines(View *v) {
  int cs = (SW - U) / MN, x0, y0 = TOP, i, flags = 0, y, bw = (SW - U - U / 3) / 2, bh = 2 * U + U / 4; char sub[80], t[4];
  (void)v;
  if (cs > (SH - U - bh - TOP) / MN) cs = (SH - U - bh - TOP) / MN;
  x0 = (SW - MN * cs) / 2;
  if (!miInit) mi_new();
  for (i = 0; i < MN * MN; i++) if (miSt[i] == 2) flags++;
  if (miState == 1) snprintf(sub, sizeof(sub), "Has trepitjat una mina"); else if (miState == 2) snprintf(sub, sizeof(sub), "Has guanyat!");
  else snprintf(sub, sizeof(sub), "%d mines · %d banderes", MINES, flags);
  header("Buscamines", sub);
  for (i = 0; i < MN * MN; i++) {
    int x = x0 + (i % MN) * cs, yy = y0 + (i / MN) * cs;
    if (miSt[i] == 1) {
      if (miMine[i]) disc(x + cs / 2, yy + cs / 2, cs / 4, BLACK);
      else { int n = mi_adj(i); if (n) { t[0] = (char)('0' + n); t[1] = 0; SetFont(fBig, BLACK); DrawTextRect(x, yy, cs, cs, t, ALIGN_CENTER | VALIGN_MIDDLE); } }
    } else {
      FillArea(x + 3, yy + 3, cs - 6, cs - 6, LGRAY);
      if (miSt[i] == 2) { FillArea(x + cs / 2 - 2, yy + cs / 4, 4, cs / 2, BLACK); FillArea(x + cs / 2, yy + cs / 4, cs / 4, cs / 5, BLACK); FillArea(x + cs / 3, yy + cs * 3 / 4 - 3, cs / 3, 4, BLACK); }
      if (miState == 0) add_hit(x, yy, cs, cs, A_MI_CELL, i);
    }
    DrawRect(x, yy, cs + 1, cs + 1, BLACK);
  }
  y = y0 + MN * cs + U / 2;
  button(U / 2, y, bw, bh, miFlagMode ? "Mode: bandera" : "Mode: destapar", miFlagMode, A_MI_MODE, 0);
  button(U / 2 + bw + U / 3, y, bw, bh, "Joc nou", 0, A_MI_NEW, 0);
}
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      thick(x + s / 2, y + s / 6, x + s / 2, y + s * 5 / 6, s / 16 > 3 ? s / 16 : 4, BLACK); thick(x + s / 6, y + s / 2, x + s * 5 / 6, y + s / 2, s / 16 > 3 ? s / 16 : 4, BLACK);
      thick(x + s / 4, y + s / 4, x + s * 3 / 4, y + s * 3 / 4, s / 16 > 3 ? s / 16 : 4, BLACK); thick(x + s * 3 / 4, y + s / 4, x + s / 4, y + s * 3 / 4, s / 16 > 3 ? s / 16 : 4, BLACK);
      disc(x + s / 2, y + s / 2, s / 4, BLACK); disc(x + s * 7 / 16, y + s * 7 / 16, s / 20, WHITE);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_MINES: v_mines(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_MI_CELL:
      if (miState != 0 || arg < 0 || arg >= MN * MN) break;
      if (miFlagMode) { if (miSt[arg] == 0) miSt[arg] = 2; else if (miSt[arg] == 2) miSt[arg] = 0; }
      else if (miSt[arg] == 0) mi_reveal(arg);
      draw(0);
      break;
    case A_MI_MODE: miFlagMode = !miFlagMode; draw(0); break;
    case A_MI_NEW: mi_new(); draw(1); break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "buscamines", "Buscamines", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
