/* Escriptori — paquet «Conversor». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_CONV };
enum { A_CONV_CAT = 100, A_CONV_UNIT, A_CONV_VAL };
/* ═════════════ Conversor ═════════════ */
typedef struct { const char *name; double f; } Unit;
static const Unit UL[] = { { "km", 1000 }, { "m", 1 }, { "cm", 0.01 }, { "mm", 0.001 }, { "milles", 1609.344 }, { "peus", 0.3048 }, { "polzades", 0.0254 } };
static const Unit UW[] = { { "kg", 1 }, { "g", 0.001 }, { "lliures", 0.45359237 }, { "unces", 0.028349523125 } };
static const Unit UV[] = { { "l", 1 }, { "ml", 0.001 }, { "galons EUA", 3.785411784 }, { "unces líq.", 0.0295735295625 } };
static const Unit UT[] = { { "°C", 0 }, { "°F", 0 }, { "K", 0 } };
static const struct { const char *name; const Unit *u; int n; } CATS[4] = { { "Longitud", UL, 7 }, { "Pes", UW, 4 }, { "Volum", UV, 4 }, { "Temp.", UT, 3 } };
static int cvCat, cvFrom; static double cvVal = 1;
static double conv(int cat, int from, int to, double v) {
  if (cat == 3) {
    double c = from == 0 ? v : from == 1 ? (v - 32) * 5 / 9 : v - 273.15;
    return to == 0 ? c : to == 1 ? c * 9 / 5 + 32 : c + 273.15;
  }
  return v * CATS[cat].u[from].f / CATS[cat].u[to].f;
}
static void conv_set(const char *t) { cvVal = parse_num(t); }
static void v_conv(View *v) {
  int y = TOP, gap = U / 4, bw = (SW - U - 3 * gap) / 4, bh = 2 * U, i, x, rh = 2 * U + U / 4; char num[48], line[120];
  (void)v;
  header("Conversor", "Toca el valor per canviar-lo");
  for (i = 0; i < 4; i++) button(U / 2 + i * (bw + gap), y, bw, bh, CATS[i].name, i == cvCat, A_CONV_CAT, i);
  y += bh + U / 2;
  x = U / 2; SetFont(fBold, BLACK);
  for (i = 0; i < CATS[cvCat].n; i++) {
    int w = StringWidth(CATS[cvCat].u[i].name) + U;
    if (x + w > SW - U / 2) { x = U / 2; y += bh + gap; }
    button(x, y, w, bh, CATS[cvCat].u[i].name, i == cvFrom, A_CONV_UNIT, i);
    SetFont(fBold, BLACK);
    x += w + gap;
  }
  y += bh + U / 2;
  fmt_num(num, sizeof(num), cvVal);
  snprintf(line, sizeof(line), "%s %s", num, CATS[cvCat].u[cvFrom].name);
  box(U / 2, y, SW - U, 4 * U);
  SetFont(fHuge, BLACK); DrawTextRect(U, y, SW - 2 * U, 4 * U, line, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
  add_hit(U / 2, y, SW - U, 4 * U, A_CONV_VAL, 0);
  y += 4 * U + U / 2;
  for (i = 0; i < CATS[cvCat].n; i++) {
    if (i == cvFrom) continue;
    fmt_num(num, sizeof(num), conv(cvCat, cvFrom, i, cvVal));
    SetFont(fBig, BLACK); DrawTextRect(U / 2, y, SW - U - 7 * U, rh, num, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
    SetFont(fText, BLACK); DrawTextRect(SW - U / 2 - 6 * U - U / 2, y, 6 * U + U / 2, rh, CATS[cvCat].u[i].name, ALIGN_LEFT | VALIGN_MIDDLE);
    DrawLine(U / 2, y + rh - 1, SW - U / 2, y + rh - 1, LGRAY);
    y += rh;
  }
}
static void mod_icon(int x, int y, int s) { ic_path(IC_CONVERT, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) {
  switch (v->type) {
    case V_CONV: v_conv(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_CONV_CAT: cvCat = arg; cvFrom = 0; draw(0); break;
    case A_CONV_UNIT: cvFrom = arg; draw(0); break;
    case A_CONV_VAL: { char num[48]; fmt_num(num, sizeof(num), cvVal); ask("Valor", num, conv_set); } break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API_MIN, "conversor", "Conversor", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
