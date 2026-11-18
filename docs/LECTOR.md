# Paquet «Lector» (EPUB)

Biblioteca i lector de llibres EPUB per a l'Escriptori. Fitxer: `src/apps/lector.c`; el motor de pàgines és el
CREngine (el del KOReader i del Kobalt), amb el pont a `src/apps/lector/` (vegeu-ne `LLEGEIX.md`). La interfície de
lectura segueix el prototip «2.4 · Lector» (Lectura, Menú, Text, Disposició i Capítols), amb les icones de `icons.h`
(`IC_LK_*`).

## Què fa

- **Biblioteca**: cerca fitxers `.epub` a tot el lector (fins a 4 nivells de carpetes, sense `system/`) i els mostra com a
  targetes amb títol, autora i progrés. A dalt, «Continua llegint» amb l'últim llibre obert. Les metadades es guarden a
  `system/config/escriptori_lector_lib.txt`.
- **Lectura** (pantalla completa, sense la barra d'estat): el terç esquerre torna enrere, el terç dret avança i el centre
  obre el menú. Segons «Capçalera i peu», a dalt hi ha el títol del llibre i a baix el capítol i «pàg. X de Y · P %». Si
  la pàgina té un marcador, surt la cinta a la cantonada.
- **Menú de lectura**: a dalt, «‹ Biblioteca» (desa i tanca el llibre), el títol i l'autora, i Cerca, Marcadors i Ajustos.
  A baix, la franja (pàgina, capítol, hora i bateria; tocar-la obre o plega Navega), el desplaçador amb un punt a cada
  capítol i els botons ‹‹ ›› de capítol, i les pestanyes. Tocar la part de la pàgina que es veu tanca el menú.
  - **Capítols**: l'índex paginat, amb el número dels capítols de primer nivell, la pàgina i «ets aquí».
  - **Navega**: tres miniatures (anterior, la triada i següent). El desplaçador i les miniatures dels costats canvien la
    vista prèvia; tocar la del mig hi va.
  - **Text**: predefinits (Escriptori, Llibresc, Modern, Del llibre; «Tots» en mostra el detall), la lletra (llista de
    fonts del motor, amb «Fonts del llibre»), mida en punts, interlineat, alineació (esquerra, justificat, la del llibre)
    i partició de mots.
  - **Disposició**: marges laterals i de dalt i baix, capçalera i peu (net, capítol i pàgina, complet), refresc total de la
    tinta (sempre, cada 6, cada 12 pàgines o mai) i estils del llibre (CSS i fonts del llibre).
- **Cerca**: busca un text al llibre (fins a 200 resultats) i en mostra un tros, el capítol i la pàgina.
- **Marcadors**: marca o desmarca la pàgina actual; llista dels del llibre per anar-hi o esborrar-los. Es desen a
  `escriptori_lector_marcadors.txt` (camí, posició XPointer, capítol, hora).
- **Ajustos**: actualitzar la biblioteca, esborrar la memòria cau de pàgines compostes i les llicències del motor.
- **Progrés i configuració**: la posició (XPointer, independent de la mida de lletra) es desa a cada canvi de pàgina a
  `escriptori_lector.txt`; la configuració, a `escriptori_lector_cfg.txt` (format «v2»; el del Lector 5 i 6 es converteix).

## Límits coneguts

- Sense verificar al lector: la velocitat de les miniatures (cada una compon una pàgina sencera), que les fonts de
  `/ebrmain/fonts` es trobin amb el nom que dona el motor (si no, les mostres «Aa» surten amb la lletra del sistema) i la
  lletra monoespaiada de la franja.
- Els canvis de pàgina amb els botons físics (si n'hi ha) no apliquen el «Refresc total»; els tocs, sí.
- No admet EPUB amb DRM.

## Provar-lo al simulador

```
./build_sim.sh && python3 tools/mkindex.py src/packages.txt sim/net/repo/packages
mkdir -p sim/flash/Llibres sim/flash/system/config sim/flash/system/escriptori/packages sim/flash/applications
cp test/llibres/*.epub sim/flash/Llibres/ && cp sim/net/repo/packages/lector.pkg sim/flash/system/escriptori/packages/
cd sim && ./uitest < ../test/tour_lector.cmd && python3 ../src/shim/render.py l*.txt
```
