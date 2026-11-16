# Avisos: bateria baixa, actualització de Notícies, campana, llista, marcar llegit, esborrar. Cal un flash net (sim/flash buit).
tap Paquets
tap Actualitza el catàleg
tap Instal·la-ho tot
xy 150 40
echo --- sense avisos: no hi ha campana
shot av_0.txt
batt 14
tick
echo --- bateria baixa: apareix la campana amb 1
shot av_1.txt
tap Notícies
tap Actualitza
echo --- després de Notícies: 2 avisos
xy 667 40
shot av_2.txt
tap Notícies actualitzades
tap Bateria baixa
echo --- tots dos llegits
shot av_3.txt
tap Bateria baixa
echo --- esborrat el llegit
shot av_4.txt
tap Esborra-ho tot
shot av_5.txt
