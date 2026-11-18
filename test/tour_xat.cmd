# Cal un sim/flash nou. Xat: obre el paquet, engega el servidor i escriu un missatge des del lector. Per provar la part web, mentre corre:
#   curl http://127.0.0.1:8080/            (pàgina)
#   curl -X POST --data-binary $'Nom\nHola' http://127.0.0.1:8080/s     (envia)
#   curl 'http://127.0.0.1:8080/m?s=0'     (missatges)
# «poll» fa córrer el temporitzador del servidor i «sleep N» espera N mil·lisegons.
tap Paquets
tap Actualitza el catàleg
tap Instal·la-ho tot
xy 1010 149
tapp Xat
shot x1.txt
sleep 1500
poll
sleep 1500
poll
kbd Hola des del lector
tap Escriu un missatge
shot x2.txt
