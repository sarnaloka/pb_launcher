# Lector: motor CREngine

Des de la versió 4 del paquet, el Lector compon les pàgines amb el **CREngine**: el motor del KOReader, el mateix que fa
servir el Kobalt (<https://github.com/nicoverbruggen/kobalt>). El Lector hi afegeix la biblioteca, la interfície i els ajustos
en català.

- `cre.h`, `cre.cpp`: el pont en C entre el Lector i el CREngine.
- `data.S`: hi posa dins del paquet el CSS per defecte del CREngine i els diccionaris de partició de mots (català, castellà,
  anglès, francès, italià, portuguès, occità, basc i gallec). La primera vegada s'escriuen a `system/escriptori/lector/`.
- Les biblioteques es compilen amb `tools/cre/build_deps.sh` (versions fixades al guió; el codi es baixa fora del
  repositori). Els pegats de l'Escriptori al CREngine són a `tools/cre/patches/`: només fan que compili amb el gcc 6.3 del
  SDK (sense `if constexpr` ni `if` amb declaració, que són de C++17).
- Totes les biblioteques s'enllacen estàticament i queden amagades dins del paquet (`-fvisibility=hidden`,
  `--exclude-libs,ALL`): el lector carrega el seu propi FreeType, més antic, i no hi han de topar. La libstdc++ també hi va
  estàtica.
- Les fonts són les del lector (`/ebrmain/fonts`) i les que hi afegeixis (`system/fonts`).

## Llicències

Com que el paquet inclou el CREngine, el paquet Lector (`../lector.c` i aquest directori) es distribueix amb la
**GPL v2 o posterior**. El paquet compilat inclou:

| Component | Llicència |
|---|---|
| CREngine (fork del KOReader, commit `4ce1aed`, el del Kobalt) | GPL v2 o posterior |
| FreeType 2.13.3 | FreeType License o GPL v2 |
| HarfBuzz 8.5.0 | MIT («Old MIT») |
| libunibreak 6.1 | zlib |
| utf8proc 2.9.0 | MIT |
| xxHash 0.8.3 | BSD de 2 clàusules |
| zlib 1.3.1 | zlib |
| libpng 1.6.43 | libpng / PNG Reference Library License |
| libjpeg-turbo 3.0.4 | IJG, BSD de 3 clàusules i zlib |
| Diccionaris de partició de mots (projecte hyph-utf8) | els de cada fitxer (LPPL, MIT…), indicats a la capçalera de cada `.pattern` |
