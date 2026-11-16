/* Escriptori — paquet «Calculadora». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_CALC };
enum { A_CALC = 100 };
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

static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      FillArea(x + s / 4, y + s / 8, s / 2, s * 3 / 4, BLACK);
      FillArea(x + s / 4 + s / 16, y + s / 8 + s / 16, s / 2 - s / 8, s / 6, WHITE);
      for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) FillArea(x + s / 4 + s / 16 + j * (s / 8 + s / 64), y + s / 8 + s * 5 / 16 + i * (s / 8 + s / 64), s / 10, s / 10, WHITE);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_CALC: v_calc(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_CALC: calc_key(arg); calc_display(); PartialUpdate(U / 2, TOP, SW - U, 4 * U); break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "calculadora", "Calculadora", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
