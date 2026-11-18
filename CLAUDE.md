# Escriptori per a PocketBook — guia per a sessions de Claude

Projecte personal: un llançador modular per a un PocketBook Verse Pro. La propietària parla català; respon-li en català
i escriu en català tot el text de la interfície.

## Què és cada cosa

- `src/base.c`, `src/base_gen.h`: sistema base (inici amb preferits, barra d'estat, tauler ràpid, avisos, gestor de paquets, Configuració amb «Sobre el dispositiu», actualització).
- `src/mod.h`: interfície entre el sistema base (`Host`) i els paquets (`Module`, `Widget`, `StatusItem`). `ESC_API` n'és la versió (7; els paquets declaren `ESC_API_MIN` i comproven `H->api` abans d'usar funcions noves). Inclou les formes, la paleta del tema (`PAL`) i els components; `src/ui.h` hi afegeix els components amb text i tacte (files, interruptors, rajoles). `src/icons.h` té les icones (camins SVG del prototip) i `src/gfx.h` les dibuixa (`ic_path`).
- `src/apps/<id>.c`: un fitxer per paquet (Bus AMB té a més `src/apps/busamb/`; el Doom, `src/apps/doom/`, codi GPL v2 del PBDoom amb els canvis llistats a `LLEGEIX.md`, que es compila a part sense avisos; el Lector, `src/apps/lector/`, el pont amb el motor CREngine, vegeu-ne `LLEGEIX.md`). `src/packages.txt`: nom, versió i descripció de cada paquet.
- `packages/`: paquets compilats (`.pkg`) i catàleg `index.txt`. `Escriptori.app` i `version.txt`: sistema base publicat.
- `tools/cre/build_deps.sh`: compila el CREngine i les seves biblioteques (versions fixades; el codi es baixa a `$CRE_SRC`, per defecte `~/cre-src`) a `build/cre-arm` o `build/cre-sim`. Els guions de compilació el criden sols si falta. Els pegats al CREngine són a `tools/cre/patches/`.
- `src/jobs.h`: fil de treball per a les descàrregues en segon pla (libcurl carregada amb dlopen; si no hi és o falla el TLS, descàrrega bloquejant).
- `src/shim/`: substitut de la biblioteca gràfica per provar a l'ordinador. `test/`: guions de prova i dades d'exemple.
- `docs/NOVA_APP.md`: com crear un paquet nou (components, widgets, ranures d'estat i avisos).

## Flux de treball

1. Canvia el codi.
2. Prova al simulador: `./build_sim.sh` (en 32 bits, com el lector: cal `gcc-multilib` i `g++-multilib`), després executa `sim/uitest < test/<guió>.cmd` des de `sim/` (cal l'estructura
   `sim/flash/system/config`, `sim/flash/system/escriptori/packages` i `sim/flash/applications` (sense `system/` les instal·lacions fallen en silenci) i, per a la xarxa simulada, `sim/net/repo/` amb el catàleg: `python3 tools/mkindex.py src/packages.txt sim/net/repo/packages`, i torna-ho a fer després de cada `build_sim.sh`, o les sumes de control no coincidiran).
   Al simulador les descàrregues es fan a l'instant; amb `ASYNC=1` passen pel fil, i `SIMNETMS=n` hi afegeix n ms de retard. `tapp text` pagina fins que troba el text; `mt x y` i `mtup` simulen un dit (multitàctil) i `polls n` executa n voltes dels temporitzadors.
   La prova del Doom (`test/tour_doom.cmd`) vol un simulador net amb els paquets instal·lats (`tour_store`) i un WAD a `sim/flash/Doom` (p. ex. `freedoom1.wad`; no és al repositori).
   Una app que s'ha deixat oberta es reprèn on era: per començar-la de zero, tanca-la (tauler ràpid › Apps obertes).
   El simulador es compila amb comprovadors de memòria: no ha de sortir cap error. `src/shim/render.py` converteix les pantalles en imatges (16 grisos, mapes de bits, inversió; les fonts amb «Serif» al nom surten amb serifa); mira-les.
3. Compila per al lector amb `build.sh` i el SDK oficial:
   `git clone --depth 1 --branch 6.5 https://github.com/pocketbook/SDK_6.3.0` (uns 2,6 GB) i `SDK=<camí>/SDK-B288 ./build.sh`.
4. Publica: fes commit i push a `main`. El lector llegeix directament de `raw.githubusercontent.com/sarnaloka/pb_launcher/main/`.

## Regles de versions

- **Paquet canviat**: puja'n la versió (enter) a `src/packages.txt`; `build.sh` regenera `packages/index.txt`.
- **Sistema base canviat**: puja `APP_BUILD` (enter creixent) i, si escau, `APP_VERNAME` a `src/base.c`, i escriu les novetats
  a la línia 4 de `version.txt` abans de compilar.
- **Interfície `mod.h` canviada de manera incompatible**: puja `ESC_API` i recompila i republica tots els paquets alhora.
- No treguis `transition_padding` de `base.c` fins que la propietària confirmi que el seu lector ja té la 0.8 o posterior.

## Límits que cal tenir presents i dir clarament

- Res no es pot executar al lector des d'una sessió: només es prova al simulador. Digues sempre què s'ha provat i què no.
- La barra d'estat només mostra estat (tocar-la obre el tauler ràpid); enrere i inici són botons rodons a la capçalera que dibuixa `header()`: tota pantalla d'un paquet ha de cridar `header()` o oferir una altra manera de sortir.
- Pantalla de tinta electrònica en blanc i negre, 1072×1448: sense animacions, botons grans, llistes paginades. Les descàrregues bloquegen el programa: l'indicador de xarxa només canvia amb una actualització parcial abans i després.
- Les funcions del sistema (xarxa, obrir fitxers, llançar altres apps) no estan verificades al lector fins que ella ho confirmi.
- El repositori és públic: no hi posis dades personals. Les dades de l'usuari viuen al lector, a `system/config/`.
- Les icones són les del prototip de disseny (`src/icons.h`): una icona nova es dibuixa a 24 × 24 només amb traços i s'hi afegeix el camí. No copiïs logotips ni icones d'altres sistemes.
- Temes: res de grisos fixos per a fons i targetes, fes servir `PAL(...)`. El tema fosc inverteix la pantalla sencera en acabar de dibuixar.
