# Escriptori per a PocketBook — guia per a sessions de Claude

Projecte personal: un llançador modular per a un PocketBook Verse Pro. La propietària parla català; respon-li en català
i escriu en català tot el text de la interfície.

## Què és cada cosa

- `src/base.c`, `src/base_gen.h`: sistema base (escriptori, barra d'estat, gestor de paquets, pantalla Sistema, actualització).
- `src/mod.h`: interfície entre el sistema base (`Host`) i els paquets (`Module`). `ESC_API` n'és la versió.
- `src/apps/<id>.c`: un fitxer per paquet (Bus AMB té a més `src/apps/busamb/`). `src/packages.txt`: nom, versió i descripció de cada paquet.
- `packages/`: paquets compilats (`.pkg`) i catàleg `index.txt`. `Escriptori.app` i `version.txt`: sistema base publicat.
- `src/shim/`: substitut de la biblioteca gràfica per provar a l'ordinador. `test/`: guions de prova i dades d'exemple.
- `docs/NOVA_APP.md`: com crear un paquet nou.

## Flux de treball

1. Canvia el codi.
2. Prova al simulador: `./build_sim.sh`, després executa `sim/uitest < test/<guió>.cmd` des de `sim/` (cal una carpeta
   `sim/flash/` i, per a la xarxa simulada, `sim/net/repo/` amb el catàleg: `python3 tools/mkindex.py src/packages.txt sim/net/repo/packages`).
   El simulador es compila amb comprovadors de memòria: no ha de sortir cap error. `src/shim/render.py` converteix les pantalles en imatges; mira-les.
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
- Pantalla de tinta electrònica en blanc i negre, 1072×1448: sense animacions, botons grans, llistes paginades.
- Les funcions del sistema (xarxa, obrir fitxers, llançar altres apps) no estan verificades al lector fins que ella ho confirmi.
- El repositori és públic: no hi posis dades personals. Les dades de l'usuari viuen al lector, a `system/config/`.
- Dibuixa icones pròpies amb formes simples; no copiïs logotips ni icones d'altres sistemes.
