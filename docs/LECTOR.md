# Paquet «Lector» (EPUB)

Biblioteca i lector de llibres EPUB per a l'Escriptori. Fitxer: `src/apps/lector.c`.

## Què fa

- **Biblioteca**: cerca fitxers `.epub` a tot el lector (fins a 4 nivells de carpetes, sense `system/`) i els mostra com a targetes
  amb títol, autora i progrés. A dalt, «Continua llegint» amb l'últim llibre obert. Les metadades es guarden a
  `system/config/escriptori_lector_lib.txt` perquè la segona vegada no cal tornar-les a llegir.
- **Lectura**: toca el terç esquerre de la pantalla per anar enrere i la resta per avançar. La barra de sota
  (capítol, pàgina i percentatge) obre el menú. Cada sis pàgines hi ha un refresc complet de la pantalla.
- **Menú**: mida de la lletra (8 nivells), tipus (sense serifa / amb serifa), interlineat, marges, continguts,
  anar a un percentatge i tancar el llibre. La configuració es desa a `escriptori_lector_cfg.txt`.
- **Progrés**: la posició de cada llibre (capítol i lloc dins del capítol) es desa a cada canvi de pàgina a
  `escriptori_lector.txt`, així que sobreviu al canvi de mida de lletra i a tancar l'Escriptori.

## Com està fet

Tot és dins d'un sol fitxer, sense cap biblioteca externa:

1. `inflate_raw` (deflate, basat en l'algorisme de «puff») i un lector de ZIP.
2. Lectura de `container.xml`, de l'OPF (metadades, espinada) i dels continguts (`nav` d'EPUB 3 o NCX d'EPUB 2).
3. `html_convert` passa cada capítol d'XHTML a blocs de text (paràgraf, títol, cita, llista, centrat); els ancoratges
   (`id`) es recorden per saltar des dels continguts.
4. `layout_all` talla el text en línies amb `StringWidth` i `paginate` en fa pàgines. Només hi ha un capítol en memòria.

## Límits coneguts

- No mostra imatges, cursiva ni negreta dins del text; les imatges soles (portades) es salten i les amb text alternatiu
  surten com `[imatge: …]`. Les taules es mostren com a text seguit.
- No parteix paraules per síl·labes ni justifica el text.
- No admet ZIP64, EPUB amb DRM ni capítols de més de 6 MB descomprimits.
- Els botons físics de canvi de pàgina no hi són (el Verse Pro no en té); el canvi de pàgina és només amb tacte.
- Sense verificar al lector: les fonts `LiberationSerif`/`LiberationSerif-Bold` (si no hi són, es fa servir la normal) i el
  rendiment de la composició d'un capítol molt llarg.

## Provar-lo al simulador

```
python3 tools/mk_test_epub.py test/llibres          # ja hi són exemple.epub i exemple2.epub
./build_sim.sh
mkdir -p sim/flash/Llibres sim/flash/system/config sim/flash/system/escriptori/packages
cp test/llibres/*.epub sim/flash/Llibres/ && cp sim/net/repo/packages/lector.pkg sim/flash/system/escriptori/packages/
cd sim && ./uitest < ../test/tour_lector.cmd && python3 ../src/shim/render.py l*.txt
```
