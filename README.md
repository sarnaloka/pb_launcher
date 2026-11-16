# Escriptori per a PocketBook

Un petit llançador ("launcher") modular per a lectors PocketBook, pensat per al Verse Pro.
No és oficial ni té relació amb PocketBook ni amb l'AMB.

## Com està organitzat

- **Sistema base** (`Escriptori.app`): escriptori amb icones, barra d'estat, gestor de paquets i actualització del sistema.
- **Paquets** (`packages/*.pkg`): cada aplicació és un mòdul que el sistema base baixa d'aquest repositori i carrega.
  El catàleg és `packages/index.txt`.

Paquets disponibles: Bus AMB, Rellotge, Notes, Calculadora, Fitxers, Tasques, Conversor, Agenda, Sudoku, Buscamines,
El temps i Notícies.

## Instal·lació

1. Baixa `Escriptori.app` i copia'l per USB a la carpeta `applications` del lector.
2. Obre **Escriptori**, connecta el Wi-Fi, entra a **Paquets**, prem *Actualitza el catàleg* i després *Instal·la-ho tot*
   (o tria els paquets un per un).

Sense Wi-Fi també es poden instal·lar paquets: copia els fitxers `.pkg` per USB a
`system/escriptori/packages/` i torna a obrir l'Escriptori.

## Actualitzacions

- **Sistema**: a *Sistema*, prem *Comprova si hi ha una versió nova*. Es llegeix `version.txt`, es baixa `Escriptori.app`
  i només se substitueix si la mida i la suma de control (CRC32) coincideixen.
- **Paquets**: a *Paquets*, *Actualitza el catàleg* i després *Instal·la i actualitza-ho tot*.

Formats:

- `version.txt`: compilació (enter creixent), mida, CRC32 hexadecimal, novetats, nom de la versió.
- `packages/index.txt`: una línia per paquet, `id|nom|versió|mida|crc32|descripció`.

## Crear un paquet

Un paquet és una biblioteca compartida que exporta `escriptori_module()` i retorna una estructura `Module`
(vegeu `src/mod.h`): nom, icona, funció de dibuix, gestor d'accions i, opcionalment, "enrere" i "obrir".
El sistema base li passa una estructura `Host` amb les mides de pantalla, les tipografies i funcions comunes
(botons, capçalera, teclat, confirmacions, descàrregues). Els exemples més curts són `src/apps/rellotge.c`
i `src/apps/calculadora.c`. Per publicar-lo: afegeix-lo a `src/packages.txt`, compila i puja `packages/`.

## Compilació i proves

- `build.sh`: compila el sistema base i tots els paquets amb el SDK oficial de PocketBook (branca 6.5, B288)
  i regenera `packages/index.txt` i `version.txt`.
- `build_sim.sh`: compila una versió per a ordinador amb un substitut mínim de la biblioteca gràfica (`src/shim/`).
  Els guions de `test/` recorren les pantalles; `src/shim/render.py` les converteix en imatges.
- `tools/gen.js`: regenera les dades de Bus AMB (`src/apps/busamb/amb.dat`) a partir del `google_transit.zip` de l'AMB.

El codi es prova al simulador; el comportament real només es pot comprovar al lector.
