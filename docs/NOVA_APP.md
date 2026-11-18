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
- Icones de línia: `ic_signal`, `ic_sun`, `ic_refresh`, `ic_check`, `ic_chevron`, `ic_plus`, `ic_minus`, `ic_grid`, `ic_arrow`, `ic_arc`. Totes reben el centre, la meitat de la mida i el color.
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


## Avisos (ESC_API 3)

El sistema base guarda els avisos a `system/config/escriptori_avisos.txt` (màx. 40) i els mostra amb una campana i un comptador a la barra d'estat, al tauler ràpid i a la vista «Avisos». Un paquet n'afegeix un amb:

```c
if (H->api >= 3 && H->notify) H->notify("Notícies", "Notícies actualitzades", "3 titulars nous.");
```

- `notify(origen, títol, text)`: si ja hi ha un avís sense llegir amb el mateix origen i títol, el substitueix. No refresca la pantalla.
- `status_changed()`: avisa que l'estat d'una ranura de la barra ha canviat; el sistema repinta només la barra.
- Compatibilitat: els paquets que només usen `notify` declaren `ESC_API_MIN` al `Module` i comproven `H->api >= 3`, així continuen carregant-se en un sistema base antic.

## Barra d'estat: refresc per peces

Cada peça del sistema (hora, bateria, Wi-Fi, xarxa, avisos) té una signatura d'estat (`sb_sig`). `sb_refresh()` repinta la barra a la memòria i només actualitza la pantalla als rectangles de les peces que han canviat; si res ha canviat, no hi ha cap refresc. El canvi de minut només refresca l'hora, i `EVT_NET_CONNECTED/DISCONNECTED` només el Wi-Fi.

## Interfície 2.0

- **Navegació**: la barra d'estat ja no té botons. `header(títol, subtítol)` dibuixa el botó rodó d'enrere a l'esquerra i el d'inici a la dreta; crida'l a totes les pantalles. El peu de paginació (botons rodons i «N de M») el dibuixa el sistema a partir de `pages`.
- **Icona del paquet**: de línia, amb el gruix comú `ic_t(s)`. Fes servir `ol()` (rectangle arrodonit de contorn), `ring()`, `rays()` i `thick()`; negre sobre blanc, ocupant uns 2/3 de la mida `s`. El sistema hi posa la rajola al voltant.
- **Botons**: `button()` és una píndola; `inv = 1` per a l'acció principal (negre).
- **Compatibilitat**: declara `ESC_API_MIN` al `Module` si no fas servir res de l'API 3; així el paquet també carrega en un sistema base anterior.

## Paquets amb feina de fons (ESC_API 4)

Un paquet pot tenir un temporitzador propi (`SetWeakTimer("<id>_poll", fn, ms)`, d'un sol tret: torna'l a armar dins `fn`) que continua mentre l'Escriptori és obert, encara que es vegi una altra pantalla. Regles:

- `H->front()` retorna l'id del paquet que es veu. Crida `H->redraw(0)` només si ets tu; si no, fes servir `H->notify(...)` i `H->status_changed()`.
- Registra una neteja amb `H->on_unload("<id>", fn)`: el sistema la crida abans de descarregar el paquet (actualització o desinstal·lació). Hi has d'aturar el temporitzador (`ClearTimer`) i tancar connexions; si no, el lector es penjarà.
- No redibuixis mentre tens un teclat o un diàleg obert.
- Tot això només existeix si `H->api >= 4`: comprova-ho i mostra un missatge si el sistema és anterior. Vegeu `src/apps/xat.c` (servidor web local per al xat) i `src/apps/ftp.c` (servidor FTP; atén ràfegues d'ordres dins un sol tic perquè connectar-se no trigui).
- Al simulador, `poll` executa el temporitzador i `sleep N` espera N ms (`test/tour_xat.cmd`).

## Aparença 2.2 (ESC_API 5)

- **Icones**: són camins SVG de traç (viewBox 24 × 24) a `src/icons.h`, els mateixos del prototip de disseny. La icona
  del paquet és una línia: `static void mod_icon(int x, int y, int s) { ic_path(IC_NOTE, x, y, s, 1.8, ICON_FG, ICON_BG); }`.
  `ICON_FG`/`ICON_BG` són els colors que el sistema demana per a aquella rajola (en un sistema antic, negre sobre blanc).
  Per a una icona nova: dibuixa-la a 24 × 24 només amb traços i afegeix-ne l'atribut `d` a `icons.h`. `ic_path` suavitza
  les vores barrejant amb el color de fons que li passis; si el fons no és uniforme, passa -1.
- **Colors**: fes servir `PAL(P_BG)`, `PAL(P_CARD)`, `PAL(P_SUB)`, `PAL(P_TILE)`, `PAL(P_LINE)`, `PAL(P_ACC)`… en lloc de
  grisos fixos. `ui_card` ja dibuixa targetes blanques amb ombra (UI_LINE), grises (UI_SOFT) o d'accent (UI_SOLID).
  El text principal pot continuar sent `BLACK`.
- **Tema fosc**: el sistema dona la paleta invertida i inverteix tota la pantalla en acabar de dibuixar; per això el
  `BLACK`/`WHITE` dels paquets també canvia. Si actualitzes una zona pel teu compte, crida `PartialUpdate` com sempre:
  `mod.h` la redirigeix a `H->update`, que inverteix la zona abans. Redibuixa sempre tota la zona que actualitzes.
- **Barra d'estat**: ja no és negra. A les ranures, el fons és `SB_BG` (negre si el sistema és anterior a l'API 5).
- **Fonts**: la font i la mida que tria la persona arriben pels `fText`, `fBold`… del sistema; no desis els punters en
  variables pròpies, perquè canvien quan es tria una altra font. Les fonts de la carpeta «Fonts» de la memòria es copien a
  `system/fonts`; el fons d'imatge de l'inici és la primera imatge de la carpeta «Fons».

## Multitasca 2.3 (ESC_API 6)

Tot això només existeix si `H->api >= 6`; comprova-ho abans d'usar-ho.

- **Apps obertes**: en prémer inici, el sistema desa la pila de pantalles del paquet i en fa una miniatura. Quan
  es torna a obrir, el paquet continua on era. Si el paquet exporta
  `void escriptori_resume(char *buf, int n)`, el text que hi escrigui (p. ex. «Nota: Llista de la compra») surt
  sota el nom a «Apps obertes». Les dades del paquet continuen vives mentre està obert; `on_unload` només es crida
  quan es tanca o es desinstal·la.
- **Descàrregues en segon pla**: `H->fetch(url, títol, cb, ctx)` posa la descàrrega a la cua del fil de treball
  i el programa no es bloqueja. `cb(ctx, data, len)` es crida al fil principal, potser abans que `fetch` torni; `data`
  és NULL si ha fallat, i el sistema l'allibera després de `cb`. La barra d'estat mostra el progrés, i es pot
  cancel·lar des de Configuració › Feina de fons. Si el paquet es tanca o es desinstal·la, `cb` ja no es crida.
  Si el lector no té libcurl o falla el TLS, la descàrrega es fa com abans (bloquejant), sense canviar res al paquet.
- **Sincronització automàtica**: `H->bg_register(id, títol, cada_minuts, start)`. `start()` es crida al fil principal quan
  toca, només amb el Wi-Fi ja connectat (i, si la persona ho tria, només carregant). Dins `start` fes servir
  `H->fetch`. Vegeu `src/apps/temps.c` i `src/apps/noticies.c`.
- **Feina pròpia**: `H->busy(id, "Servidor actiu")` fa sortir el paquet com a «treballant» al monitor;
  `H->busy(id, NULL)` ho treu.
- **Pantalla dividida**: un paquet hi pot sortir si exporta
  `__attribute__((visibility("default"))) const int escriptori_split = 1;`. A la meitat, `SH` és l'alçada de la meitat,
  `SB` val 0 i `H->oy` és on comença verticalment. `FillArea`, `DrawRect`, `DrawLine`, `DrawPixel`, `DrawBitmap`,
  `InvertArea` i `DrawTextRect` ja hi sumen `H->oy`, i `header()` i els tocs també el tenen en compte. Dibuixa sempre
  entre 0 i `SH`. Si crides directament una altra funció de dibuix de l'InkView, suma-hi `ESC_OY`.

## Exportacions opcionals (ESC_API 7)

- `int escriptori_event(int type, int par1, int par2)`: el paquet rep els tocs directes (`EVT_POINTERDOWN`, `UP`, `DRAG`,
  `MOVE`, `LONG`, `HOLD` i el multitàctil `EVT_MTSYNC`) quan és a primer pla, abans que el sistema miri els botons. Si torna
  1, el sistema no el tracta. No arriba res amb el tauler ràpid obert, a la pantalla dividida ni amb un diàleg.
  Deixa passar (torna 0) els tocs de la capçalera (`y < TOP`), si no, no es podrà sortir.
- `const int escriptori_reload = 1;`: en tancar-lo des d'«Apps obertes» (o el monitor), el sistema el descarrega i el torna
  a carregar, perquè la propera vegada comenci amb les variables netes. Allibera la memòria a `on_unload`.
- Un paquet que ho necessiti declara `api = 7` al `Module`: un sistema anterior no el carregarà.
- Vegeu `src/apps/doom.c`.
