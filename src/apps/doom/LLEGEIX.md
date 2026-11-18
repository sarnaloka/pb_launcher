# Doom per a l'Escriptori

Aquest directori conté el codi del **PBDoom 0.0.5** d'Ilgiz Mustafin (<https://github.com/imustafin/pbdoom>), basat en
**SDL Doom 1.10** de Sam Lantinga i en el codi font del **Doom** que id Software va publicar el 1997. Tot es distribueix
amb la **GPL v2** (vegeu `LICENSE`). El paquet de l'Escriptori (`../doom.c`) s'hi enllaça i, per tant, també és GPL v2.

Al repositori no hi ha cap fitxer WAD. Cal copiar-ne un a la carpeta `Doom` de la memòria del lector (o a
`applications/pbdoom`, on el posa el PBDoom original): `doom1.wad` (shareware), `doom.wad`, `doomu.wad`, `doom2.wad`,
`plutonia.wad`, `tnt.wad`, o els lliures `freedoom1.wad` i `freedoom2.wad` (<https://freedoom.github.io>).

## Canvis respecte de l'original

Fets per adaptar-lo a l'Escriptori (2026). Al codi estan marcats amb «Escriptori».

- `esc_glue.h`, `glue.c` (nous): la unió amb l'Escriptori (actualitzacions de pantalla, pausa, errors, alliberar memòria).
- `pbdoom_state.c`: el bucle només corre mentre el Doom és a primer pla; un error (`I_Error`) hi torna amb `longjmp`.
- `pbdoom/gui.c`, `gui.h`: sense el plafó superior ni el menú del PBDoom (l'Escriptori hi posa la seva capçalera i una
  pantalla d'ajustos); el joc i els controls ocupen la zona que dona l'Escriptori.
- `pbdoom/frames/panel.c`, `panel.h`: eliminats.
- `pbdoom/frames/game.c`, `game.h`: cada fotograma es dibuixa amb un sol mapa de bits ampliat (abans, un `FillArea` per píxel);
  en tema fosc es pinta invertit; constants en `#define` per al compilador del SDK.
- `pbdoom/frames/keyboard.c`: textos en català; botons nous «Menú» i «Enrere» (Esc); allibera la font.
- `pbdoom/frames/color_settings.c`: textos en català.
- `sdldoom-1.10/i_system.c`: `I_Quit` i `I_Error` ja no tanquen el programa.
- `sdldoom-1.10/d_main.c`: Freedoom; configuració i partides a la carpeta del WAD; un `malloc` amb un byte de menys.
- `sdldoom-1.10/g_game.c`, `m_menu.c`: partides desades a la carpeta del WAD; sense l'espera d'1,5 s en sortir.
- `sdldoom-1.10/info.c`, `info.h`: `sprnames` acabat en `NULL` (`R_InitSpriteDefs` el busca).
- `sdldoom-1.10/r_main.c`: angles sense signe a `R_ScaleFromGlobalAngle` (índex negatiu a `finesine`).
- `sdldoom-1.10/tables.c`, `tables.h`: `finetangent` i `finesine` en una sola taula, com eren a la memòria de l'original
  (`R_RenderSegLoop` llegeix `finetangent` més enllà del final).
- `sdldoom-1.10/m_misc.c`: les cadenes de la configuració en memòria fixa.

El codi del Doom de 1997 no funciona en 64 bits: el simulador de l'Escriptori es compila amb `-m32`.
