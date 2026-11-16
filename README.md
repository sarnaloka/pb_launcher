# Escriptori per a PocketBook

Un petit llançador ("launcher") amb una suite d'eines per a lectors PocketBook, pensat per al Verse Pro.
No és oficial ni té relació amb PocketBook ni amb l'AMB.

## Què inclou

- **Bus AMB**: parades, sortides i horaris teòrics del bus metropolità, amb exportació a PDF. Les dades són el GTFS obert de l'AMB.
- Rellotge i calendari, notes, calculadora, fitxers, tasques, conversor d'unitats, agenda, sudoku, buscamines, el temps i notícies (RSS).
- Barra d'estat pròpia amb navegació, cobertura Wi-Fi, bateria i hora.
- Les aplicacions `.app` que hi hagi a la carpeta `applications` del lector surten com a icones.

## Instal·lació

1. Baixa `Escriptori.app` d'aquest repositori.
2. Connecta el lector per USB i copia el fitxer a la carpeta `applications`.
3. Desconnecta'l i obre **Escriptori** des de la llista d'aplicacions.

## Actualització per Wi-Fi

Dins de l'Escriptori, obre **Actualitza** i prem *Comprova si hi ha una versió nova*.
L'app llegeix `version.txt` d'aquest repositori, baixa `Escriptori.app`, en comprova la mida i la suma de control (CRC32)
i només llavors substitueix la versió instal·lada.

Format de `version.txt`: versió, mida en bytes, CRC32 en hexadecimal i una línia de novetats.

## Compilació

Cal el SDK oficial de PocketBook (branca 6.5, plataforma B288). Vegeu `build.sh`.
`src/shim/` conté un substitut mínim de la biblioteca gràfica per provar la interfície en un ordinador.
`tools/gen.js` regenera `src/amb.dat` a partir del `google_transit.zip` de l'AMB.

## Estat

El codi es prova en un simulador d'ordinador; el comportament real només es pot comprovar al lector.
