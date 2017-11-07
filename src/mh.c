/*
 * mh.c
 *
 *  Created on: 27 ott 2017
 *      Author: Flavio Bucceri
 *     Version: 1.1.1  (03 nov 2017)
 *
 */
#include <stdio.h>
#include <time.h>
#include <mosquitto.h>
#include "kmlgen.h"
#include "string.h"
#include <errno.h>
#include <unistd.h>

#define DEBUG
#ifdef DEBUG
#define PDBG(...) printf(__VA_ARGS__)
#else
#define PDBG(...) /* NOP */
#endif

#define MIN_DELTA_SAVE 10.0

extern KMLInfo kmlInfo;
extern LampData lampData[];
extern int numItems;

bool ordered = true;

time_t timeLastSaved;
bool updated = false;
bool running = true;
double saveDelay;

struct mosquitto *mosq = NULL;
char *axmj_in = "/axlight/f48e30d0-566f-4524-9130-1d65f17d8a53/stc/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/axmj/";
char *cmd_in = "/axlight/f48e30d0-566f-4524-9130-1d65f17d8a53/stc/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/cmdin/";
char strMqttMsg[50000];

/**
 * Validates and parses a string as an unsigned integer
 * @param arg
 * @return
 */
unsigned int parseArgument(const char * arg) {
	char c;
	int val;
	int n = 0;
	while ((c = *(arg + n)) != '\0') {
		if (c < '0' || c > '9') {
			fprintf(stderr, "Formato inatteso del parametro: %s\n", arg);
			return 0;
		}
		n++;
	}
	val = atoi(arg);
	return val;
}

void LoadGeomapFile(const char *fpath) {
	FILE *fp;
	if ((fp = fopen(fpath, "r+")) != NULL) {
		char * lastmacaddr[17];
		lastmacaddr[0] = '\0';
		int n;
		for (n = 0; n != MAX_LAMPS && fscanf(fp, "%*6[^;];%35[^;];%16[^;];%15[^;];%15[^;];%15[^|]|", lampData[n].nome, lampData[n].macaddr, lampData[n].coord1, lampData[n].coord2, lampData[n].coord3) != EOF; n++) {
			PDBG("[debug] %s %s\n", lampData[n].nome, lampData[n].macaddr);
			if (strcmp(lastmacaddr, lampData[n].macaddr) > 0) {
				ordered = false;
			}
			strcpy(lastmacaddr, lampData[n].macaddr);
		}
		if (!ordered) {
			PDBG("Avviso: voci in Geomap.txt non ordinate per mac address crescente\n");
		}
		numItems = n;
		fclose(fp);
	} else {
		err(1, "File %s", fpath);
	}
}

LampData *linsearch_macaddr(const char *macaddr, LampData *lamp, int n) {
	int i = n;
	while (i != 0) {
		PDBG("[debug] comparing with %s\n", lamp->macaddr);
		if (strcmp(macaddr, lamp->macaddr) != 0) {
			lamp++;
			i--;
		} else {
			PDBG("[debug] found: %s\n", macaddr);
			return lamp;
		}
	}
	return NULL;
}

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
			PDBG("[debug] found: %s", mid->macaddr);
			return mid;
		}
	}
	return NULL;
}

void ReadDimmerFromMQTTMessage(char data[]) {
	char macaddr[17], power[4];
	int n;
	const char *pch = data;
	while (sscanf(pch, "%*[#.!]NMEAS;MAC%16[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];PW1%[^;];%*[^#!]%n", macaddr, power, &n) == 2) {
		/* usa ricerca binaria sui macaddr se ordinati */
		PDBG("[debug] Searching for: %s\n", macaddr);
		LampData *lamp = NULL;
		if (ordered) { /* TODO puntatore a funzione */
			lamp = binsearch_macaddr(macaddr, lampData, numItems);
		} else {
			lamp = linsearch_macaddr(macaddr, lampData, numItems);
		}
		if (lamp != NULL) {
			PDBG("[debug] Dimmer %s\n", power);
			if (strcmp(lamp->pw1, power) != 0) { /* aggiorna solo se diverso */
				strcpy(lamp->pw1, power);
				updated = true;
			}
		}
		pch += n;
	}
}

/*static const char* delim = "#.!\n\r";
 void ReadDimmerFromMQTTMessage2(char mqtt_message[]) {
 char* token, *macaddr, *pch;
 token = strtok(mqtt_message, delim);
 while (token != NULL) {
 fprintf(stderr, "\n[dbg]TOKEN=%s\n", token);
 macaddr = strstr(token, ";MAC") + 4;
 fprintf(stderr, "\n[dbg]MAC=%s\n", macaddr);
 pch = strchr(macaddr, ';');
 *pch = '\0';
 usa ricerca binaria sui macaddr se ordinati
 LampData *lamp;
 if (ordered) {  TODO puntatore a funzione
 lamp = binsearch_macaddr(macaddr, lampData, numItems);
 } else {
 lamp = linsearch_macaddr(macaddr, lampData, numItems);
 }
 if (lamp != NULL) {
 fprintf(stderr, "\nRicevuto %s\n", macaddr);
 macaddr = strstr(pch + 1, ";PW1") + 4;
 pch = strchr(macaddr, ';');
 *pch = '\0';
 unsigned int value = atoi(macaddr);
 if (lamp->pw1 != value) {
 lamp->pw1 = value;
 updated = true;
 }
 }
 }
 token = strtok(NULL, delim);
 }*/

void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str) {
	/* Pring all log messages regardless of level. */
	printf("%s\n", str);
}

/* Callback alla connessione che effettua iscrizione ai topic */
void my_connect_callback(struct mosquitto *mosq, void *userdata, int rc) {
	if (rc == 0) {
		mosquitto_subscribe(mosq, NULL, axmj_in, 1); /* messaggi */
		mosquitto_subscribe(mosq, NULL, cmd_in, 2); /* comandi */
	} else {
		fprintf(stderr, "mqtt connect failure %d\n", rc);
	}
	fflush(stderr);
}

void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
	int i;
	printf("Subscribe: (mid: %d): %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++) {
		printf(", %d", granted_qos[i]);
	}
	fputc('\n', stderr);
	fflush(stderr);
}

void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	int i = 1;
	PDBG("%s <= %s\n", message->topic, (char *) message->payload);
	char * pch[10];
	pch[i] = strtok(message->topic, "/");
	while (pch[i] != NULL) {
		i++;
		pch[i] = strtok(NULL, "/");
	}
	if (strncmp(pch[1], "axlight", 7) == 0) {
		if (strncmp(pch[3], "stc", 3) == 0) {
			if (strncmp(pch[4], "nde", 3) == 0) {
				if (strncmp(pch[6], "axmj", 4) == 0) {
					if (message->payload != NULL) {
						strcpy(strMqttMsg, message->payload);
						ReadDimmerFromMQTTMessage(strMqttMsg);
					}
				} else if (strncmp(pch[6], "cmdin", 5) == 0) { /* FIXME convalida logica */
					PDBG("\nMessaggio arrivato: %s-%s\n", pch[6], (char *) message->payload);
					if (strncmp("QUIT", message->payload, 4) == 0) {
						running = false;
					} else if (strcmp("WAIT", message->payload) == 0) {
						saveDelay = 0;
						fputs("\nSospesa generazione KML\n", stderr);
					} else {
						int val = parseArgument(message->payload);
						if (val > 0) {
							running = true;
							saveDelay = val;
							printf("\nNuovo intervallo aggiornamento KML: %d secondi\n", val);
						} else {
							printf("Comando sconosciuto: %s", message->payload);
						}
					}
				}
			}
		}
		fflush(stderr);
	}
}

int main2(int argc, char *argv[]) { /* TEST */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	kmlInfo.autore = "mh-1.0.0";
	kmlInfo.name = "MQTTHeatmap";
	LoadGeomapFile("/www/Geomap.txt");
	strcpy(strMqttMsg,
			"#.#NMEAS;MAC00158D00011B9C7D;IDN56;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9CAE;LQI78;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER5;PW0900;PW1900;PW2900;TMP29;VCC3214;AD01;AD11;AD22;AD320;MOS4;#!##.#NMEAS;MAC00158D00011B1387;IDN70;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B138B;LQI150;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3183;AD01;AD11;AD21;AD3599;MOS1;#!##.#NMEAS;MAC00158D000109EDE4;IDN113;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B132B;LQI138;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3193;AD01;AD11;AD21;AD3599;MOS2;#!##.#NMEAS;MAC00158D000109EA5D;IDN48;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B1384;LQI45;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER3;PW0900;PW1900;PW2900;TMP29;VCC3180;AD02;AD12;AD22;AD320;MOS2;#!##.#NMEAS;MAC00158D00011B1327;IDN41;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9C7D;LQI168;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3187;AD01;AD12;AD22;AD3599;MOS4;#!##.#NMEAS;MAC00158D00011B132B;IDN23;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D000109EA5B;LQI171;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3180;AD01;AD11;AD21;AD318;MOS2;#!#");
	ReadDimmerFromMQTTMessage(strMqttMsg);
	int i;
	for (i = 0; i < numItems; i++) {
		printf("%s mac=%s pw1=%s\n", lampData[i].nome, lampData[i].macaddr, lampData[i].pw1);
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 2) {
		puts("Nota: i parametri in eccesso verranno ignorati.\n");
	}
	if (argc == 2) {
		saveDelay = parseArgument(argv[1]);
	}
	if (saveDelay == 0) {
		puts("Impostato valore di default");
		saveDelay = MIN_DELTA_SAVE;
	}
	printf("Periodo aggiornamento Heatmap.kml impostato a %.2f secondi\n", saveDelay);

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

	if (mosquitto_connect(mosq, "127.0.0.1"/*host*/, 1883/*port*/, 60/*timeout_sec*/) != MOSQ_ERR_SUCCESS) {
		err(1, "Mosquitto connect");
	}

	mosquitto_loop_start(mosq);
	running = true;
	updated = false;
	timeLastSaved = 0;

	while (running) {
		if (!updated && saveDelay != 0 && difftime(time(NULL), timeLastSaved) > saveDelay) {
			WriteKMLFile("/mnt/sd/Heatmap.kml");
			timeLastSaved = time(NULL);
			updated = true;
		}
		sleep(1);
	}
	mosquitto_disconnect(mosq);
	mosquitto_loop_stop(mosq, false);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return 0;
}
