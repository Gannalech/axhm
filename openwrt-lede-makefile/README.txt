Questi sono i due Makefile per compilare usando LEDE (oppure openWRT, al momento cambia solo il nome).

a) "Makefile_target" compila il codice.
b) "Makefile_utils" crea il pacchetto ipk.

Seguire l'iter descritto per la creazione di un servizio. (TODO: aggiungerla qui)
I Makefile vanno messi rispettivamente sotto lede/target/... e lede/utils/... e rinominati in "Makefile"
Occorre probabilmente correggere il path di libmosquitto.so.1 (la versione installata può variare).
