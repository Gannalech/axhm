Come creare un pacchetto ipk per il servizio mh (wip)
-----------------------------------------------------

Una volta strutturato correttamente un repository git o svn come feed e' 
possibile aggiungere il feed alla propria installazione di lede
e tutto avviene in automatico. Non avendo per il momento un repository con
queste caratteristiche occorre fare alcune operazioni a mano.

- Partire da una installazione di lede configurata.

- Creare la cartella lede/dl/mh-1.0.1 e copiarci dentro i sorgenti C da src/

- Prendere i Makefile sotto opnwrt-lede-makefile/

a) "Makefile_target" compila il codice => copiare sotto lede/package/ax/mh
b) "Makefile_ipk" crea il pacchetto ipk => copiare sotto lede/dl/mh-1.0.1

Vanno rinominati entrambi in "Makefile".

- Eseguire alcuni comandi a terminale.
# crea archivio da cui parte il giro di compilazione:
cd dl
tar -cvzf mh-1.0.1.tar.gz mh-1.0.1 
# Tornare nella home di lede
cd ..
./scripts/feeds update -a
./scripts/feeds install mh
# Configura compilazione: sotto la voce Allix -> mh mettere (M)
make menuconfig
# Avviare la compilazione del pacchetto
make -j1 V=s package/mh/compile
