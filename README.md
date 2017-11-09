# AxHeatMaps

Inizio lettura file di configurazione con informazioni `MAC_ADDRESS/NOME LAMPADA/COORDINATE`

Caricamento vettore di strutture 150 con dati letti (Manca info del valore dimmer).

## LETTURA DATI DA CODA MQTT

`void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)`

all'interno della callback lettura messaggio con più dati delle lampade e riempimento struttura dati con valore DIMMER (campo **PW1**).

messa status MODIFICATO su struttura condivisa.

## ROUTINE CICLO
Esegue l'operazione di scrittura XML.
```
if(status MODIFICATO) {
  do parsing stringa message;
  chiamo WriteXML;
  messa status OK;
}
```
## ESEMPIO GEOMAP (file informazioni sulle lampade)

`ID000;GW HeatMap;00158D00019A5E07;0.0034936043;0.0045115078;0|ID001;Lamp 1;00158D00010A4C57;0.0032502170;0.0043601978;1|ID002;Lamp 2;00158D00010A4C47;0.0034158439;0.0043624436;1|ID003;Lamp 3;00158D00010A48EC;0.0035820322;0.0043607593;1|ID004;Lamp 4;00158D00010A4C67;0.0037889254;0.0043610400;1|`

carattere | per separare i record.

ID000;GW HeatMap;00158D00019A5E07;0.0034936043;0.0045115078;0

quindi ; per separare all'interno dei campi:

```
ID000 --> ???
GW HeatMap --> Nome lampada
00158D00019A5E07 --> Indirizzo MAC

0.0034936043;0.0045115078 --> due coordinate

0 --> ???
```
## ESEMPIO DATI LAMPADE

```
#.#NMEAS;MAC00158D00011B9C7D;IDN56;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9CAE;LQI78;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER5;PW0900;PW1900;PW2900;TMP29;VCC3214;AD01;AD11;AD22;AD320;MOS4;#!#
#.#NMEAS;MAC00158D00011B1387;IDN70;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B138B;LQI150;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3183;AD01;AD11;AD21;AD3599;MOS1;#!#
#.#NMEAS;MAC00158D000109EDE4;IDN113;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B132B;LQI138;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3193;AD01;AD11;AD21;AD3599;MOS2;#!#
#.#NMEAS;MAC00158D000109EA5D;IDN48;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B1384;LQI45;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER3;PW0900;PW1900;PW2900;TMP29;VCC3180;AD02;AD12;AD22;AD320;MOS2;#!#
#.#NMEAS;MAC00158D00011B1327;IDN41;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9C7D;LQI168;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3187;AD01;AD12;AD22;AD3599;MOS4;#!#
#.#NMEAS;MAC00158D00011B132B;IDN23;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D000109EA5B;LQI171;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3180;AD01;AD11;AD21;AD318;MOS2;#!#
```
```
MAC - è il mac address della lampada
IDN - è il numero id della luce all'interno del coordinatore attaccato al gateway
PW0 PW1 PW2 - sono i valori di dimmerazione delle luci (da 0 a 1000) 
MOS - la zona impostata di quella lampada, in questo modo è possibile settare diversi gruppi di lampade
; - separatore

#.#NMEAS - start token
#!# - end token
```

## FILE ESEMPI

File di esempio sotto `feeds_test`

* _Geomap.txt_ esempio di file di mappaggio informazione lamapade;
* _Misure.txt_ esempio di messaggio pubblicato con informazioni lamapade - l'applicazione deve leggere e parsare questo messaggio; 
