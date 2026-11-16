# Crear un paquet nou per a l'Escriptori

Un paquet és un fitxer `src/apps/<id>.c` que es compila com a biblioteca compartida (`packages/<id>.pkg`).

## Esquelet mínim

```c
#define ESC_MODULE
#include "../mod.h"
enum { V_MAIN };                 /* vistes del paquet, la 0 és la d'entrada */
enum { A_TOCA = 100 };           /* accions pròpies: sempre a partir de 100 */

static int comptador;
static void v_main(View *v) {
  char t[40]; (void)v;
  header("Exemple", "Un paquet mínim");
  snprintf(t, sizeof(t), "Has tocat %d vegades", comptador);
  SetFont(fText, BLACK);
  DrawTextRect(U / 2, TOP, SW - U, 2 * U, t, ALIGN_LEFT | VALIGN_MIDDLE);
  button(U / 2, TOP + 3 * U, SW - U, 2 * U + U / 2, "Toca'm", 0, A_TOCA, 0);
}
static void mod_icon(int x, int y, int s) { disc(x + s / 2, y + s / 2, s / 3, BLACK); }
static void mod_draw(View *v) { switch (v->type) { case V_MAIN: v_main(v); break; } }
static void mod_action(View *v, int a, int arg) { (void)v; (void)arg; if (a == A_TOCA) { comptador++; draw(0); } }
static void mod_open(View *v) { (void)v; }
static Module M = { ESC_API, "exemple", "Exemple", 0, mod_icon, mod_draw, mod_action, NULL, mod_open };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
```

## El que et dona `mod.h`

- Mides: `SW`, `SH` (pantalla), `U` (unitat de mida), `TOP` i `BOTTOM` (zona útil), `NAVH`.
- Tipografies: `fTitle`, `fBold`, `fText`, `fSmall`, `fBig`, `fHuge`.
- Dibuix i tacte: `header`, `button`, `box`, `disc`, `thick`, `add_hit(x, y, w, h, acció, argument)`.
- Navegació: `push(vista, a, b, sel, text)`, `POP()`, `draw(1)` (refresc complet) o `draw(0)` (suau), `pages` per a la paginació del peu.
- Diàlegs: `ask(títol, inicial, funció)` obre el teclat; `confirm(títol, text, botó, funció)`; `say(text)` mostra un avís.
- Xarxa i fitxers: `net_get(url, &mida)`, `read_file`, i `Store` (`st_load`, `st_save`, `st_insert`, `st_del`) per a llistes d'una línia per element.
- Dades de l'usuari: desa-les a `CFG "escriptori_<id>.txt"`.

## Passos per publicar-lo

1. Afegeix una línia a `src/packages.txt`: `id|Nom|1|Descripció curta.`
2. Afegeix un guió a `test/` que n'obri totes les pantalles i prova'l amb `build_sim.sh`.
3. Compila amb `build.sh`, fes commit i push. Al lector apareixerà a Paquets després d'actualitzar el catàleg.

Exemples reals, de més curt a més llarg: `rellotge.c`, `calculadora.c`, `tasques.c`, `agenda.c`, `noticies.c`.
