# Doom. Cal el paquet instal·lat (tour_store o copiar doom.pkg) i un WAD a sim/flash/Doom (p. ex. freedoom1.wad, de
# freedoom.github.io; no és al repositori). Juga, surt a l'inici (pausa), torna, ajustos, tema fosc, surt pel menú i tanca'l.
tapp Doom
polls 3
shot dm0.txt
# menú (Esc), joc nou, episodi, dificultat
mt 445 1300
polls 2
mtup
mt 950 1250
polls 2
mtup
polls 2
mt 950 1250
polls 2
mtup
polls 2
mt 950 1250
polls 2
mtup
polls 40
shot dm1.txt
# camina endavant amb el joystick i dispara
mt 180 1150
polls 20
mtup
mt 900 1300
polls 3
mtup
polls 5
shot dm2.txt
# inici: queda obert en pausa; el bucle s'atura
xy 1010 149
poll
poll
shot dm3.txt
tapp Doom
polls 3
shot dm4.txt
# ajustos: mode de pantalla
xy 900 149
shot dm5.txt
tap Grisos
tap Ràpid
xy 62 149
polls 2
# tema fosc des del tauler ràpid, i de nou clar
xy 600 40
xy 402 215
polls 3
shot dm6.txt
xy 300 1200
polls 3
shot dm6b.txt
xy 600 40
xy 402 215
xy 300 1200
polls 2
# surt pel menú del Doom: Menú, avall x5 (Quit Game), Tria, Sí
mt 445 1300
polls 2
mtup
mt 177 1320
polls 1
mtup
mt 177 1320
polls 1
mtup
mt 177 1320
polls 1
mtup
mt 177 1320
polls 1
mtup
mt 177 1320
polls 1
mtup
mt 950 1250
polls 2
mtup
polls 2
shot dm7.txt
mt 513 1100
polls 2
mtup
polls 3
shot dm8.txt
# torna a entrar: la demostració del títol; i el tanca des d'Apps obertes (es torna a carregar)
tapp Doom
polls 3
xy 1010 149
xy 600 40
tap Apps obertes
shot dm9.txt
tap Tanca-les totes
xy 1010 149
tapp Doom
polls 3
shot dm10.txt
