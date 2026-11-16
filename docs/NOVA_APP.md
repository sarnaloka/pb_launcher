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

## Components de disseny (ESC_API 2)

L'aspecte comú viu a `mod.h` (formes i icones) i `ui.h` (components amb text i tacte); fes-los servir en lloc de dibuixar vores a mà.

- Targetes: `ui_card(x, y, w, h, UI_LINE | UI_SOFT | UI_SOLID)`; `box()` ja és una targeta arrodonida. `rfill` i `rcard` per a formes pròpies.
- Components: `ui_row(x, y, w, h, títol, subtítol, estat, acció, argument)` (fila de llista), `ui_toggle(x, y, w, h, actiu)`, `ui_tile(...)` (rajola), `ui_lv(x, y, w, h, etiqueta, valor)` (format estàndard d'un widget d'una línia).
- Icones de línia: `ic_wifi`, `ic_sun`, `ic_refresh`, `ic_check`, `ic_chevron`, `ic_plus`, `ic_minus`, `ic_grid`, `ic_arrow`, `ic_arc`. Totes reben el centre, la meitat de la mida i el color.
- `button()` i `header()` del sistema ja segueixen aquest estil; no cal fer-hi res.

## Widgets de l'escriptori

Un paquet pot oferir peces d'informació per a l'escriptori. Declara'ls i passa'ls al final de `Module`:

```c
static void w_exemple(int x, int y, int w, int h) { ui_lv(x, y, w, h, "Exemple", "Hola"); }
static const Widget WG[] = { { "hola", "Widget d'exemple", 2, 1, w_exemple } };   /* id, nom, columnes (2 o 4), alçada (1-3) */
static Module M = { ESC_API, "exemple", "Exemple", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 1, WG };
```

El sistema dibuixa la targeta i crida `draw` amb l'interior. En tocar-lo s'obre el paquet. Els dibuixa de manera que els de 2 columnes s'aparellen; l'usuari tria quins es veuen i en quin ordre a «Personalitza l'inici» (tauler ràpid), i la selecció es desa a `system/config/escriptori_inici.txt` amb claus `paquet.widget`. Els widgets per defecte són `base.rellotge`, `temps.avui`, `tasques.pendents` i `agenda.proper`.

## Ranures de la barra d'estat

Un indicador petit a la barra superior: `StatusItem { id, width, draw }`. `width(h)` retorna l'amplada en píxels (0 = ara no es mostra) i `draw(x, y, w, h, color)` el dibuixa. S'avalua a cada refresc de la barra, així que ha de ser barat. Vegeu `tasques.c` (comptador de tasques pendents). Es passen com `1, SI` just abans dels widgets a `Module`.

