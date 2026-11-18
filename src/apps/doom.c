/* Doom per a l'Escriptori: el PBDoom d'Ilgiz Mustafin (basat en SDL Doom de Sam Lantinga i en el codi font del Doom
   d'id Software) convertit en paquet. Aquest fitxer és la part de l'Escriptori: pantalles, arrencada, pausa i ajustos.
   Com que s'enllaça amb el Doom, es distribueix amb la GPL v2 (vegeu apps/doom/LICENSE i apps/doom/LLEGEIX.md).

   - El WAD es busca a la carpeta «Doom» de la memòria (i a applications/pbdoom, on el posa el PBDoom original).
   - El joc avança amb un temporitzador que només corre mentre el Doom és a primer pla: en sortir queda en pausa.
   - Els tocs arriben directament (escriptori_event, API 7) per al joystick multitàctil.
   - Si el Doom troba un error greu, es torna aquí (longjmp) en lloc de tancar l'Escriptori; per engegar-lo de nou
     es tanca des d'«Apps obertes» i el sistema torna a carregar el paquet (escriptori_reload). */
#define ESC_MODULE
#include "../mod.h"
#include <setjmp.h>
#include <unistd.h>
#include "doom/esc_glue.h"
#undef PartialUpdate                           /* aquí cal la de l'InkView: la inversió del tema fosc ja la fa esc_doom_update */

enum { V_GAME, V_SET };
enum { A_SET = 100, A_RETRY, A_MODE, A_COLORS };
enum { ST_IDLE, ST_NOWAD, ST_BOOT, ST_RUN, ST_ERR };

char esc_doom_dir[512];
jmp_buf esc_doom_jmp;
int esc_doom_jmp_ok, esc_doom_dark, esc_doom_quit_req;

static int st, curType = -1, guiDone;
static char wadName[64], errMsg[300];

static const char *DIRS[] = { FLASHDIR "/Doom", FLASHDIR "/applications/pbdoom", NULL };
static const char *WADS[] = { "doom2f.wad", "doom2.wad", "plutonia.wad", "tnt.wad", "doomu.wad", "doom.wad", "doom1.wad", "freedoom2.wad", "freedoom1.wad", NULL };
static const char *WADNAME[] = { "DOOM II (francès)", "DOOM II", "Final DOOM: Plutonia", "Final DOOM: TNT", "The Ultimate DOOM", "DOOM", "DOOM (shareware)", "Freedoom: Phase 2", "Freedoom: Phase 1", NULL };

/* Busca el WAD: la mateixa preferència que el Doom (IdentifyVersion), a cada carpeta per ordre. */
static int find_wad(void) {
  int d, w; char p[700];
  for (d = 0; DIRS[d]; d++) for (w = 0; WADS[w]; w++) {
    snprintf(p, sizeof(p), "%s/%s", DIRS[d], WADS[w]);
    if (access(p, R_OK) == 0) { snprintf(esc_doom_dir, sizeof(esc_doom_dir), "%s", DIRS[d]); snprintf(wadName, sizeof(wadName), "%s", WADNAME[w]); return 1; }
  }
  return 0;
}
/* Tema fosc de l'Escriptori: el paquet no el rep per la interfície, es llegeix de la configuració. */
static void read_dark(void) {
  FILE *f = fopen(CFG "escriptori_config.txt", "r"); char ln[200];
  esc_doom_dark = 0;
  if (!f) return;
  while (fgets(ln, sizeof(ln), f)) if (!strncmp(ln, "tema=", 5)) esc_doom_dark = atoi(ln + 5) == 1;
  fclose(f);
}

/* ───── unió amb el Doom ───── */
int esc_doom_active(void) {
  const char *f = H->api >= 4 && H->front ? H->front() : "doom";
  return st == ST_RUN && curType == V_GAME && f && !strcmp(f, "doom");
}
void esc_doom_update(int x, int y, int w, int h, int a2) {
  if (esc_doom_dark) InvertArea(x, y, w, h);
  if (a2) DynamicUpdateA2(x, y, w, h); else PartialUpdate(x, y, w, h);
}
static void after_error(void) { draw(1); }
void esc_doom_failed(const char *msg) {
  snprintf(errMsg, sizeof(errMsg), "%s", msg && msg[0] ? msg : "error desconegut");
  st = ST_ERR;
  SetWeakTimer("doom_err", after_error, 10);       /* es redibuixa fora del bucle del Doom */
}
void esc_doom_quit_done(void) { POP(); draw(1); }
void D_DoomLoop_iter_pbdoom();
static void boot(void) {
  static char a0[] = "doom", *argv[] = { a0, NULL };
  if (st != ST_BOOT) return;
  if (setjmp(esc_doom_jmp)) { esc_doom_jmp_ok = 0; return; }
  esc_doom_jmp_ok = 1;
  setenv("DOOMWADDIR", esc_doom_dir, 1);
  if (!guiDone) { esc_doom_gui(0, TOP, SW, SH - TOP, SW >= 1000 && SH >= 1400 ? 300 : 212); guiDone = 1; }
  st = ST_RUN;
  doom_main(1, argv);                            /* acaba engegant el bucle (start_D_DoomLoop) */
  esc_doom_jmp_ok = 0;
  draw(1);
}

/* ───── pantalles ───── */
static void mod_icon(int x, int y, int s) { ic_path(IC_DOOM, x, y, s, 1.8, ICON_FG, ICON_BG); }
/* Botó rodó de la capçalera (ajustos), al costat del d'inici. */
static void head_button(void) {
  int d = 2 * U + U / 4, y0 = SB + (3 * U - d) / 2 + U / 8, x = SW - U / 2 - d - U / 3 - d, s = d * 52 / 100;
  rfill(x, y0 + 2, d, d, d / 2, PAL(P_SHADOW)); rfill(x, y0, d, d, d / 2, PAL(P_CARD));
  ic_path(IC_SLIDERS, x + (d - s) / 2, y0 + (d - s) / 2, s, 2.2, PAL(P_FG), PAL(P_CARD));
  add_hit(x - U / 6, SB, d + U / 3, 3 * U + U / 4, A_SET, 0);
}
static void card_text(int y, int h, const char *title, const char *text) {
  ui_card(U / 2, y, SW - U, h, UI_LINE);
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U + U / 4, y + U / 2, SW - 2 * U - U / 2, U * 3 / 2, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fText, PAL(P_FG)); DrawTextRect(U + U / 4, y + 2 * U + U / 4, SW - 2 * U - U / 2, h - 3 * U, text, ALIGN_LEFT | VALIGN_TOP);
}
static void v_game(void) {
  char sub[100]; int y = TOP + U / 2, bh = 2 * U + U / 2;
  snprintf(sub, sizeof(sub), "%s", st == ST_RUN || st == ST_BOOT ? wadName : st == ST_ERR ? "S'ha aturat" : "Falta el joc");
  header("Doom", sub);
  if (st == ST_RUN || st == ST_BOOT) head_button();
  if (st == ST_IDLE || st == ST_NOWAD) {
    st = find_wad() ? ST_BOOT : ST_NOWAD;
    if (st == ST_NOWAD) mkdir(FLASHDIR "/Doom", 0777);
  }
  if (st == ST_NOWAD) {
    card_text(y, 12 * U, "Cal el fitxer del joc (WAD)",
      "Connecta el lector a l'ordinador i copia un fitxer WAD a la carpeta «Doom» de la memòria (ja l'he creada).\n\n"
      "Serveixen doom1.wad (la versió shareware), doom.wad, doomu.wad, doom2.wad, plutonia.wad i tnt.wad, i també els "
      "lliures freedoom1.wad i freedoom2.wad (freedoom.github.io). El nom ha d'anar en minúscules.");
    button(U / 2, y + 12 * U + U / 2, SW - U, bh, "Torna-ho a mirar", 1, A_RETRY, 0);
    return;
  }
  if (st == ST_ERR) {
    char t[500];
    snprintf(t, sizeof(t), "%s\n\nPer tornar-lo a engegar, tanca'l des del tauler ràpid › Apps obertes i torna'l a obrir.", errMsg);
    card_text(y, 12 * U, "El Doom s'ha aturat", t);
    return;
  }
  if (st == ST_BOOT) {
    SetFont(fText, PAL(P_SUB)); DrawTextRect(U, y + 4 * U, SW - 2 * U, 2 * U, "Carregant el Doom…", ALIGN_CENTER | VALIGN_MIDDLE);
    SetWeakTimer("doom_boot", boot, 60);
    return;
  }
  esc_doom_draw_all();
  esc_doom_resume();
}
static void v_set(void) {
  static const char *MODE[4] = { "Ràpid", "Trama", "Trama fina", "Grisos" };
  int y = TOP + U / 4, bw = (SW - U - 3 * (U / 3)) / 4, bh = 2 * U + U / 4, i, m = esc_doom_get_mode();
  char t[700];
  header("Ajustos del Doom", wadName);
  SetFont(fBold, PAL(P_FG)); DrawTextRect(U / 2, y, SW - U, U * 3 / 2, "Pantalla", ALIGN_LEFT | VALIGN_MIDDLE); y += U * 3 / 2 + U / 4;
  for (i = 0; i < 4; i++) button(U / 2 + i * (bw + U / 3), y, bw, bh, MODE[i], i == m, A_MODE, i);
  y += bh + U / 4;
  SetFont(fSmall, PAL(P_SUB));
  DrawTextRect(U / 2, y, SW - U, 3 * U, "«Ràpid» és en blanc i negre i el més fluid. Amb trama o en grisos es veu més detall, però la pantalla va més lenta i deixa més rastre.", ALIGN_LEFT | VALIGN_TOP);
  y += 3 * U + U / 2;
  button(U / 2, y, SW - U, bh, "Contrast, brillantor i gamma", 0, A_COLORS, 0); y += bh + U;
  snprintf(t, sizeof(t), "Joc: %s, de la carpeta %s. Les partides desades també hi van.\n\n"
    "PBDoom 0.0.5 d'Ilgiz Mustafin, basat en SDL Doom de Sam Lantinga i en el codi font del Doom d'id Software. "
    "Adaptat a l'Escriptori. Llicència GPL v2; el codi és a github.com/imustafin/pbdoom i github.com/sarnaloka/pb_launcher.",
    wadName, esc_doom_dir + strlen(FLASHDIR) + (esc_doom_dir[strlen(FLASHDIR)] == '/' ? 1 : 0));
  card_text(y, BOTTOM - y, "Sobre el Doom", t);
}
static void mod_draw(View *v) {
  read_dark();
  curType = v->type;
  if (v->type == V_SET) v_set(); else v_game();
}
static void mod_action(View *v, int a, int arg) {
  (void)v;
  switch (a) {
    case A_SET: push(V_SET, 0, 0, 0, NULL); break;
    case A_RETRY: st = ST_IDLE; draw(1); break;
    case A_MODE: esc_doom_set_mode(arg); draw(0); break;
    case A_COLORS: esc_doom_open_colors(); POP(); draw(1); break;
  }
}
static void mod_open(View *v) { v->type = V_GAME; }
static int mod_back(View *v) { (void)v; return 0; }

/* Tocs directes: tot el que cau sota la capçalera és del joc (els controls fan servir el multitàctil). */
__attribute__((visibility("default"))) int escriptori_event(int t, int a, int b) {
  if (st != ST_RUN || curType != V_GAME) return 0;
  if (t == EVT_MTSYNC) { esc_doom_handle(t, a, b); return 1; }
  if (b < TOP) return 0;
  esc_doom_handle(t, a, b);
  return 1;
}
__attribute__((visibility("default"))) const int escriptori_reload = 1;

static void on_unload(void) {
  ClearTimer(D_DoomLoop_iter_pbdoom); ClearTimer(boot); ClearTimer(after_error);
  if (st == ST_RUN || st == ST_ERR) esc_doom_free();
}
static Module M = { 7, "doom", "Doom", 0, mod_icon, mod_draw, mod_action, mod_back, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) {
  H = h;
  if (h->api >= 4 && h->on_unload) h->on_unload("doom", on_unload);
  return &M;
}
