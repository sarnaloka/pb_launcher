/* Escriptori — paquet «Sudoku». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_SUDOKU };
enum { A_SU_CELL = 100, A_SU_NUM, A_SU_NEW, A_SU_CHECK };
/* ═════════════ Sudoku ═════════════ */
#define SUFILE CFG "escriptori_sudoku.txt"
static char suG[81], suC[81], suSol[81]; static int suSel = -1, suReady;
static int su_cands(const char *g, int i) {
  int r = i / 9, c = i % 9, m = 0, k, br = r / 3 * 3, bc = c / 3 * 3;
  for (k = 0; k < 9; k++) { m |= 1 << g[r * 9 + k]; m |= 1 << g[k * 9 + c]; m |= 1 << g[(br + k / 3) * 9 + bc + k % 3]; }
  return ~m & 0x3FE;
}
static int su_solve(char *g, int limit, char *sol) {      /* compta solucions fins a `limit` */
  int best = -1, bestN = 10, i, m, d, count = 0;
  for (i = 0; i < 81; i++) if (!g[i]) {
    int n = 0; m = su_cands(g, i);
    for (d = 1; d <= 9; d++) if (m & (1 << d)) n++;
    if (n < bestN) { bestN = n; best = i; if (n <= 1) break; }
  }
  if (best < 0) { if (sol) memcpy(sol, g, 81); return 1; }
  m = su_cands(g, best);
  for (d = 1; d <= 9 && count < limit; d++) if (m & (1 << d)) { g[best] = (char)d; count += su_solve(g, limit - count, sol); g[best] = 0; }
  return count;
}
static void su_shuffle(int *a, int n) { int i; for (i = n - 1; i > 0; i--) { int j = rand() % (i + 1), t = a[i]; a[i] = a[j]; a[j] = t; } }
static void su_save(void) {
  FILE *f = fopen(SUFILE, "w"); int i;
  if (!f) return;
  for (i = 0; i < 81; i++) fputc('0' + suG[i], f);
  fputc('\n', f);
  for (i = 0; i < 81; i++) fputc('0' + suC[i], f);
  fputc('\n', f); fclose(f);
}
static void su_new(void) {
  int rows[9], cols[9], dig[9], bands[3] = { 0, 1, 2 }, stacks[3] = { 0, 1, 2 }, order[81], i, r, c, removed = 0;
  char g[81], tmp[81];
  for (i = 0; i < 9; i++) dig[i] = i + 1;
  su_shuffle(dig, 9); su_shuffle(bands, 3); su_shuffle(stacks, 3);
  for (i = 0; i < 3; i++) { int a[3] = { 0, 1, 2 }, b[3] = { 0, 1, 2 }; su_shuffle(a, 3); su_shuffle(b, 3); for (r = 0; r < 3; r++) { rows[i * 3 + r] = bands[i] * 3 + a[r]; cols[i * 3 + r] = stacks[i] * 3 + b[r]; } }
  for (r = 0; r < 9; r++) for (c = 0; c < 9; c++) g[r * 9 + c] = (char)dig[(rows[r] * 3 + rows[r] / 3 + cols[c]) % 9];
  memcpy(suSol, g, 81);
  for (i = 0; i < 81; i++) order[i] = i;
  su_shuffle(order, 81);
  for (i = 0; i < 81 && removed < 46; i++) {
    char keep = g[order[i]];
    g[order[i]] = 0; memcpy(tmp, g, 81);
    if (su_solve(tmp, 2, NULL) != 1) g[order[i]] = keep; else removed++;
  }
  memcpy(suG, g, 81); memcpy(suC, g, 81); suSel = -1; suReady = 1; su_save();
}
static void su_load(void) {
  FILE *f; char a[100], b[100], tmp[81]; int i, ok = 0;
  if (suReady) return;
  f = fopen(SUFILE, "r");
  if (f) {
    if (fgets(a, sizeof(a), f) && fgets(b, sizeof(b), f) && strlen(a) >= 81 && strlen(b) >= 81) {
      ok = 1;
      for (i = 0; i < 81; i++) { if (a[i] < '0' || a[i] > '9' || b[i] < '0' || b[i] > '9') ok = 0; suG[i] = (char)(a[i] - '0'); suC[i] = (char)(b[i] - '0'); }
      if (ok) { memcpy(tmp, suG, 81); if (su_solve(tmp, 1, suSol) != 1) ok = 0; }
    }
    fclose(f);
  }
  if (ok) suReady = 1; else su_new();
}
static void v_sudoku(View *v) {
  int cs = (SW - U) / 9, x0, y0 = TOP, i, left = 0, y, bw, bh = 2 * U + U / 4, gap = U / 6; char t[4], sub[64];
  (void)v;
  if (cs > (SH - U / 2 - 2 * bh - U - TOP) / 9) cs = (SH - U / 2 - 2 * bh - U - TOP) / 9;
  x0 = (SW - 9 * cs) / 2;
  su_load();
  for (i = 0; i < 81; i++) if (!suC[i]) left++;
  if (left) snprintf(sub, sizeof(sub), "Falten %d caselles", left); else snprintf(sub, sizeof(sub), memcmp(suC, suSol, 81) ? "Ple, però hi ha errors" : "Resolt!");
  header("Sudoku", sub);
  for (i = 0; i < 81; i++) {
    int x = x0 + (i % 9) * cs, yy = y0 + (i / 9) * cs;
    if (i == suSel) FillArea(x, yy, cs, cs, BLACK);
    if (suC[i]) { t[0] = (char)('0' + suC[i]); t[1] = 0; SetFont(suG[i] ? fBig : fText, i == suSel ? WHITE : BLACK); DrawTextRect(x, yy, cs, cs, t, ALIGN_CENTER | VALIGN_MIDDLE); }
    if (!suG[i]) add_hit(x, yy, cs, cs, A_SU_CELL, i);
  }
  for (i = 0; i <= 9; i++) {
    int t2 = i % 3 == 0 ? 5 : 1;
    FillArea(x0 + i * cs - t2 / 2, y0, t2, 9 * cs + 1, BLACK); FillArea(x0, y0 + i * cs - t2 / 2, 9 * cs + 1, t2, BLACK);
  }
  y = y0 + 9 * cs + U / 2; bw = (SW - U - 9 * gap) / 10;
  for (i = 1; i <= 9; i++) { t[0] = (char)('0' + i); t[1] = 0; button(U / 2 + (i - 1) * (bw + gap), y, bw, bh, t, 0, A_SU_NUM, i); }
  button(U / 2 + 9 * (bw + gap), y, bw, bh, "X", 0, A_SU_NUM, 0);
  y += bh + U / 3; bw = (SW - U - U / 3) / 2;
  button(U / 2, y, bw, bh, "Comprova", 0, A_SU_CHECK, 0); button(U / 2 + bw + U / 3, y, bw, bh, "Sudoku nou", 0, A_SU_NEW, 0);
}
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      for (i = 0; i <= 3; i++) { FillArea(x + s / 6 + i * (s * 2 / 9), y + s / 6, 4, s * 2 / 3 + 4, BLACK); FillArea(x + s / 6, y + s / 6 + i * (s * 2 / 9), s * 2 / 3 + 4, 4, BLACK); }
      FillArea(x + s / 6 + s / 14, y + s / 6 + s / 14, s / 12, s / 12, BLACK); FillArea(x + s / 6 + s * 2 / 9 + s / 14, y + s / 6 + s * 2 / 9 + s / 14, s / 12, s / 12, BLACK); FillArea(x + s / 6 + s * 4 / 9 + s / 14, y + s / 6 + s / 14, s / 12, s / 12, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_SUDOKU: v_sudoku(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
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
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "sudoku", "Sudoku", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
