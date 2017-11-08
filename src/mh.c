/*
 * mh.c
 *
 *  Created on: 27 ott 2017
 *      Author: Flavio Bucceri
 *     Version: 1.0.0  (08 nov 2017)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mosquitto.h>
#include "kmlgen.h"
#include "string.h"
#include <errno.h>
#include <err.h>
#include <unistd.h>

const char* host = "127.0.0.1";
int port = 1883;
int timeout_sec = 60;

#define MIN_DELTA_SAVE 10

extern KMLInfo kmlInfo;
extern LampData lampData[];
extern int numItems;

time_t timeLastSaved;
bool updated = false; /* arrivata misura nuova rispetto al KML salvato se gia' creato */
bool running = true; /* servizio MH in esecuzione */
double saveDelay; /* pausa minima tra due salvataggi (in secondi); 0 = sospendi generazione KML */

struct mosquitto *mosq = NULL;
char strMqttMsg[50000];

/* topic */
char *axmj_in = "/axlight/f48e30d0-566f-4524-9130-1d65f17d8a53/stc/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/axmj/";
char *cmd_in = "/axlight/f48e30d0-566f-4524-9130-1d65f17d8a53/stc/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/cmdin/";

/* Macro per i messaggi diagnostici */
#define DEBUG
#ifdef DEBUG
#define PDBG(...) printf(__VA_ARGS__)
#else
#define PDBG(...) /* NOP */
#endif

/**
 * Funzione di confronto tra LampData, usata da qsort()
 */
int comp(const void * elem1, const void * elem2) {
	char *f = ((LampData*) elem1)->macaddr;
	char *s = ((LampData*) elem2)->macaddr;
	return strcmp(f, s);
}

/**
 * Validates and parses a string as an integer
 * @param arg
 * @return
 */
int parseAsInteger(char * arg) {
	int val;
	if (sscanf(arg, "%d", &val) == 1) {
		return val;
	}
	fprintf(stderr, "Formato inatteso del parametro: %s\n", arg);
	return 0;
}

/**
 * Legge da disco il file Geomap con le anagrafiche delle lampade di interesse
 */
void LoadGeomapFile(const char *fpath) {
	FILE *fp;
	if ((fp = fopen(fpath, "r")) != NULL) {
		/* XXX fare qsort senza controllare prima l'ordinamento? */
		char lastmacaddr[17];
		lastmacaddr[0] = '\0';
		int n;
		bool ordered = true;
		for (n = 0; n != MAX_LAMPS && fscanf(fp, "%*6[^;];%35[^;];%16[^;];%15[^;];%15[^;];%15[^|]|", lampData[n].nome, lampData[n].macaddr, lampData[n].coord1, lampData[n].coord2, lampData[n].coord3) == 5; n++) {
			lampData[n].pw1[0] = '\0';
			PDBG("[debug] %s %s\n", lampData[n].nome, lampData[n].macaddr);
			if (strcmp(lastmacaddr, lampData[n].macaddr) > 0) {
				ordered = false;
			}
			strcpy(lastmacaddr, lampData[n].macaddr);
		}
		numItems = n;
		fclose(fp);
		if (!ordered) {
			qsort(lampData, numItems, sizeof(*lampData), comp);
			PDBG("Le voci lette da Geomap.txt sono state riordinate per mac address crescente\n");
		}
	} else {
		err(1, "File %s", fpath);
	}
}

/**
 * Ricerca binaria mediante puntatori per trovare un mac address
 */
LampData *binsearch_macaddr(const char *macaddr, LampData *lamp, int n) {
	int cond;
	LampData *low = &lamp[0];
	LampData *high = &lamp[n];
	LampData *mid;

	while (low < high) {
		mid = low + (high - low) / 2;
		if ((cond = strcmp(macaddr, mid->macaddr)) < 0)
			high = mid;
		else if (cond > 0)
			low = mid + 1;
		else {
			PDBG("[debug] found: %s\n", mid->macaddr);
			return mid;
		}
	}
	return NULL;
}

/**
 * Estrapola il mac address di ogni misura ricevuta, se presente tra le lampade di interesse aggiorna il PW1
 */
void ReadDimmerFromMQTTMessage(char data[]) {
	char macaddr[17], power[4];
	int n;
	const char *pch = data;
	while (sscanf(pch, "%*[#.!]NMEAS;MAC%16[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];PW1%[^;];%*[^#!]%n", macaddr, power, &n) == 2) {
		/* usa ricerca binaria sui macaddr */
		PDBG("[debug] Search for %s\n", macaddr);
		LampData *lamp = NULL;
			lamp = binsearch_macaddr(macaddr, lampData, numItems);
		if (lamp != NULL) {
			PDBG("[debug] PW1 was '%s' now '%s'\n", lamp->pw1, power);
			if (strcmp(lamp->pw1, power) != 0) { /* aggiorna solo se diverso */
				strcpy(lamp->pw1, power);
				updated = false;
			}
		}
		pch += n;
	}
}
/**
 * Callback per gestire i messaggi di log di mosquitto
 */
void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str) {
	/* Pring all log messages regardless of level. */
	printf("%s\n", str);
}

/**
 * Callback all'atto della connessione, effettua l'iscrizione ai topic
 */
void my_connect_callback(struct mosquitto *mosq, void *userdata, int rc) {
	if (rc == 0) {
		mosquitto_subscribe(mosq, NULL, axmj_in, 1); /* messaggi */
		mosquitto_subscribe(mosq, NULL, cmd_in, 2); /* comandi */
	} else {
		fprintf(stderr, "mqtt connect failure %d\n", rc);
	}
	fflush(stderr);
}

/**
 * Callback per ogni iscrizione ai topic
 */
void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
	int i;
	printf("Subscribe (mid=%d) qos: %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++) {
		printf(", %d", granted_qos[i]);
	}
	puts("\n");
	fflush(stdout);
}

/**
 * Callback per ogni messaggio ricevuto da mosquitto
 */
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	int i = 1;
	PDBG("%s <= %s\n", message->topic, (char * ) message->payload);
	char * pch[10];
	pch[i] = strtok(message->topic, "/");
	while (pch[i] != NULL) {
		PDBG("%s\n", pch[i]);
		i++;
		pch[i] = strtok(NULL, "/");
	}
	if (strncmp(pch[1], "axlight", 7) == 0) {
		if (strncmp(pch[3], "stc", 3) == 0) {
			if (strncmp(pch[4], "nde", 3) == 0) {
				if (strncmp(pch[6], "axmj", 4) == 0) {
					if (message->payload != NULL) {
						strncpy(strMqttMsg, message->payload, message->payloadlen);
						ReadDimmerFromMQTTMessage(strMqttMsg);
					}
				} else if (strncmp(pch[6], "cmdin", 5) == 0) { /* FIXME convalida logica */
					PDBG("\nMessaggio %s: %s\n", pch[6], (char * ) message->payload);
					if (strncmp("QUIT", message->payload, 4) == 0) {
						running = false;
					} else if (strcmp("OFF", message->payload) == 0) {
						saveDelay = 0;
						fputs("\nSospesa generazione KML\n", stderr);
					} else if (strcmp("ON", message->payload) == 0) {
						saveDelay = MIN_DELTA_SAVE;
						fputs("\nSospesa generazione KML\n", stderr);
					} else {
						strcpy(strMqttMsg, message->payload);
						int val = parseAsInteger(strMqttMsg);
						if (val > 0) {
							running = true;
							saveDelay = val;
							printf("\nIntervallo aggiornamento KML: %d secondi\n", val);
						} else {
							printf("Comando ignorato: %s\n", strMqttMsg);
						}
					}
				}
			}
		}
		fflush(stderr);
	}
}

/* TEST */
/*
int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	kmlInfo.autore = "mh-1.0.0";
	kmlInfo.name = "MQTTHeatmap";
	LoadGeomapFile("/www/Geomap.txt");
	strcpy(strMqttMsg,
			"#.#NMEAS;MAC00158D00010A4C57;IDN56;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9CAE;LQI78;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER5;PW0900;PW1900;PW2900;TMP29;VCC3214;AD01;AD11;AD22;AD320;MOS4;#!##.#NMEAS;MAC00158D00011B1387;IDN70;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B138B;LQI150;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3183;AD01;AD11;AD21;AD3599;MOS1;#!##.#NMEAS;MAC00158D000109EDE4;IDN113;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B132B;LQI138;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3193;AD01;AD11;AD21;AD3599;MOS2;#!##.#NMEAS;MAC00158D000109EA5D;IDN48;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B1384;LQI45;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER3;PW0900;PW1900;PW2900;TMP29;VCC3180;AD02;AD12;AD22;AD320;MOS2;#!##.#NMEAS;MAC00158D00011B1327;IDN41;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9C7D;LQI168;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3187;AD01;AD12;AD22;AD3599;MOS4;#!##.#NMEAS;MAC00158D00011B132B;IDN23;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D000109EA5B;LQI171;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3180;AD01;AD11;AD21;AD318;MOS2;#!#");
	ReadDimmerFromMQTTMessage(strMqttMsg);
	int i;
	for (i = 0; i < numItems; i++) {
		printf("%s mac=%s pw1=%s\n", lampData[i].nome, lampData[i].macaddr, lampData[i].pw1);
	}
	return 0;
}
*/
/* Entry point */
int main(int argc, char *argv[]) {
	if (argc > 2) {
		puts("Nota: i parametri in eccesso verranno ignorati.\n");
	}
	if (argc == 2) {
		saveDelay = parseAsInteger(argv[1]);
	}
	if (saveDelay == 0) {
		/* Imposta valore di default */
		saveDelay = MIN_DELTA_SAVE;
	}
	printf("Periodo aggiornamento Heatmap.kml impostato a %.0f secondi\n", saveDelay);

	kmlInfo.autore = "mh-1.0.0";
	kmlInfo.name = "MQTTHeatmap";
	LoadGeomapFile("/www/Geomap.txt");

	mosquitto_lib_init();

	if (!(mosq = mosquitto_new(kmlInfo.name, true/*clean_session*/, NULL))) {
		err(1, "Mosquitto new");
	}
	mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

	if (mosquitto_connect(mosq, host, port, timeout_sec) != MOSQ_ERR_SUCCESS) {
		err(1, "Mosquitto connect");
	}

	mosquitto_loop_start(mosq);
	running = true;
	updated = false;
	timeLastSaved = 0;

	while (running) {
		if (!updated && saveDelay != 0 && difftime(time(NULL), timeLastSaved) >= saveDelay) {
			WriteKMLFile("/mnt/sd/Heatmap.kml");
			timeLastSaved = time(NULL);
			updated = true;
		}
		sleep(saveDelay); /* attesa minima */
	}
	mosquitto_disconnect(mosq);
	mosquitto_loop_stop(mosq, false);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return 0;
}
