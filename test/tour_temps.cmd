# Cal un sim/flash nou (sense escriptori_temps_*) i test/weather.json i test/geocode.json copiats a sim/net/.
# Les dades de prova comencen el 18/11/2026: un altre dia, la previsió surt com a «antiga».
tapp El temps
shot t1.txt
tap Actualitza (cal Wi-Fi)
shot t2.txt
tap 10 dies
shot t3.txt
tap Detalls
shot t4.txt
tap Ara
tap dj. 19
shot t5.txt
key 25
shot t5b.txt
xy 62 237
xy 1010 390
kbd Girona
tap Afegeix una ciutat
shot t6.txt
tap Girona
shot t7.txt
tap Edita
shot t8.txt
tap Principal
shot t9.txt
tap Fet
tap Barcelona
shot t10.txt
xy 1010 237
shot t11.txt
