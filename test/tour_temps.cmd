# Cal un sim/flash nou (sense escriptori_temps_*) i test/weather.json i test/geocode.json copiats a sim/net/.
tap El temps
shot t1.txt
tap Actualitza (cal Wi-Fi)
shot t2.txt
tap Ciutats
kbd Girona
tap Afegeix una ciutat
shot t3.txt
tap Girona
shot t4.txt
kbd Val
tap Afegeix una ciutat
tap València
shot t5.txt
tap Girona
shot t6.txt
tap Fes-la la ciutat principal
shot t7.txt
xy 55 43
tap Edita
shot t8.txt
dlg 1
tap València
shot t9.txt
tap Fet
xy 150 40
shot t10.txt
