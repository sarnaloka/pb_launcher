/* Escriptori — paquet «Conversor». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_CONV };
enum { A_CONV_CAT = 100, A_CONV_UNIT, A_CONV_VAL, A_CONV_REFRESH };
#define DVURL "https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"   /* tipus de referència diaris del BCE (XML) */
#define DVFILE CFG "escriptori_divises.txt"                                     /* data i hora de l'última descàrrega, i després una línia per divisa */
#define DVMAXAGE (3 * 24 * 3600)                                                /* més de tres dies sense actualitzar: el subtítol ho recorda */
/* ═════════════ Conversor ═════════════ */
typedef struct { const char *name; double f; } Unit;
static const Unit UL[] = { { "km", 1000 }, { "m", 1 }, { "cm", 0.01 }, { "mm", 0.001 }, { "milles", 1609.344 }, { "milles nàut.", 1852 }, { "peus", 0.3048 }, { "polzades", 0.0254 } };
static const Unit UW[] = { { "kg", 1 }, { "g", 0.001 }, { "lliures", 0.45359237 }, { "unces", 0.028349523125 } };
static const Unit UV[] = { { "l", 1 }, { "ml", 0.001 }, { "galons EUA", 3.785411784 }, { "unces líq.", 0.0295735295625 } };
static const Unit UT[] = { { "°C", 0 }, { "°F", 0 }, { "K", 0 } };
/* Divises: f = euros per unitat. Valors de sèrie (BCE 16/11/2026); dv_apply() els canvia pels descarregats. */
static Unit UD[] = { { "EUR", 1 }, { "USD", 1 / 1.1014 }, { "GBP", 1 / 0.83450 }, { "CHF", 1 / 0.9244 }, { "JPY", 1 / 175.78 } };
static const struct { const char *name; const Unit *u; int n; } CATS[5] = { { "Longitud", UL, 8 }, { "Pes", UW, 4 }, { "Volum", UV, 4 }, { "Temp.", UT, 3 }, { "Divises", UD, 5 } };
static int cvCat, cvFrom; static double cvVal = 1;
/* ───── tipus de canvi (xarxa + còpia local) ───── */
static const char *DVCODE[5] = { "EUR", "USD", "GBP", "CHF", "JPY" };
static double dvRate[5] = { 1, 1.1014, 0.83450, 0.9244, 175.78 };       /* unitats per euro */
static char dvDate[16] = "16/11/2026";                                  /* data d'aquests tipus, DD/MM/AAAA */
static time_t dvWhen;                                                   /* quan es van descarregar (0 = només els de sèrie) */
static void dv_apply(void) { int i; for (i = 1; i < 5; i++) UD[i].f = 1 / dvRate[i]; }
static void dv_save(time_t when) {
  FILE *f = fopen(DVFILE, "w"); int i;
  if (!f) return;
  fprintf(f, "%s %ld\n", dvDate, (long)when);
  for (i = 1; i < 5; i++) fprintf(f, "%s %.6f\n", DVCODE[i], dvRate[i]);
  fclose(f);
}
static void dv_load(void) {
  FILE *f = fopen(DVFILE, "r"); char ln[80], code[8], date[16]; double r[5] = { 1, 0, 0, 0, 0 }; long when = 0; int i, seen = 0;
  if (!f) return;
  if (fgets(ln, sizeof(ln), f) && sscanf(ln, "%15s %ld", date, &when) == 2) {
    while (fgets(ln, sizeof(ln), f)) {
      double rate;
      if (sscanf(ln, "%7s %lf", code, &rate) != 2 || !(rate > 0)) continue;
      for (i = 1; i < 5; i++) if (!strcmp(code, DVCODE[i])) { r[i] = rate; seen |= 1 << i; }
    }
  }
  fclose(f);
  if (seen != 0x1E) return;                                             /* fitxer incomplet: es queden els de sèrie */
  for (i = 1; i < 5; i++) dvRate[i] = r[i];
  snprintf(dvDate, sizeof(dvDate), "%s", date);
  dvWhen = (time_t)when;
  dv_apply();
}
static int dv_fetch(void) {
  char *b, pat[40]; int len = 0, i, y, m, d, seen = 0; double r[5]; const char *p, *q; time_t now;
  b = net_get(DVURL, &len);
  if (!b) return 0;
  p = strstr(b, "<Cube time='");
  if (!p || sscanf(p, "<Cube time='%d-%d-%d'", &y, &m, &d) != 3) { free(b); return 0; }
  for (i = 1; i < 5; i++) {
    snprintf(pat, sizeof(pat), "currency='%s' rate='", DVCODE[i]);
    q = strstr(b, pat);
    if (q && (r[i] = strtod(q + strlen(pat), NULL)) > 0) seen |= 1 << i;
  }
  free(b);
  if (seen != 0x1E) return 0;
  for (i = 1; i < 5; i++) dvRate[i] = r[i];
  snprintf(dvDate, sizeof(dvDate), "%02d/%02d/%04d", d, m, y);
  now = time(NULL);
  dvWhen = now; dv_apply(); dv_save(now);
  return 1;
}
static int dv_stale(void) { return !dvWhen || time(NULL) - dvWhen > DVMAXAGE; }
static double conv(int cat, int from, int to, double v) {
  if (cat == 3) {
    double c = from == 0 ? v : from == 1 ? (v - 32) * 5 / 9 : v - 273.15;
    return to == 0 ? c : to == 1 ? c * 9 / 5 + 32 : c + 273.15;
  }
  return v * CATS[cat].u[from].f / CATS[cat].u[to].f;
}
static void conv_set(const char *t) { cvVal = parse_num(t); }
static void v_conv(View *v) {
  int y = TOP, gap = U / 4, bw = (SW - U - 4 * gap) / 5, bh = 2 * U, i, x, rh = 2 * U + U / 4; char num[48], line[120], sub[80];
  (void)v;
  if (cvCat == 4) snprintf(sub, sizeof(sub), dv_stale() ? "Tipus del BCE del %s · cal actualitzar-los" : "Tipus del BCE del %s", dvDate);
  else snprintf(sub, sizeof(sub), "Toca el valor per canviar-lo");
  header("Conversor", sub);
  for (i = 0; i < 5; i++) button(U / 2 + i * (bw + gap), y, bw, bh, CATS[i].name, i == cvCat, A_CONV_CAT, i);
  y += bh + U / 2;
  x = U / 2; SetFont(fBold, BLACK);
  for (i = 0; i < CATS[cvCat].n; i++) {
    int w = StringWidth(CATS[cvCat].u[i].name) + U;
    if (x + w > SW - U / 2) { x = U / 2; y += bh + gap; }
    button(x, y, w, bh, CATS[cvCat].u[i].name, i == cvFrom, A_CONV_UNIT, i);
    SetFont(fBold, BLACK);
    x += w + gap;
  }
  if (cvCat == 4) { y += bh + gap; button(U / 2, y, SW - U, bh, "Actualitza el tipus de canvi", 0, A_CONV_REFRESH, 0); }
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
    case A_CONV_CAT: cvCat = arg; cvFrom = 0; draw(0); break;   /* sense descàrregues automàtiques: només amb el botó, com la resta de paquets */
    case A_CONV_UNIT: cvFrom = arg; draw(0); break;
    case A_CONV_VAL: { char num[48]; fmt_num(num, sizeof(num), cvVal); ask("Valor", num, conv_set); } break;
    case A_CONV_REFRESH: {
      char msg[120];
      if (dv_fetch()) snprintf(msg, sizeof(msg), "Tipus de canvi actualitzat (BCE, %s).", dvDate);
      else snprintf(msg, sizeof(msg), "No s'ha pogut descarregar el tipus de canvi. Comprova el Wi-Fi. Es fan servir els del %s.", dvDate);
      say(msg); draw(1);
    } break;
  }
}
static void mod_open(View *v) {
  (void)v;
  dv_load();
}
static Module M = { ESC_API_MIN, "conversor", "Conversor", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
