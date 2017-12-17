/*
 * mh.c
 *
 *  Created on: 27 ott 2017
 *      Author: Flavio Bucceri
 *     Version: 1.1.0  (12 dic 2017)
 */
/**
 * @file mh.c
 * @author Flavio Bucceri
 * @date 08 nov 2017
 *	Genera periodicamente un heatmap in formato KML di lampade con anagrafica, potenza instantanea e conteggio variazioni di stato del PIR.
 *	Le anagrafiche sono lette da Geomap.txt, e corrispondono al perimetro di interesse.
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
#include <signal.h>
#include <ctype.h>

#ifndef VERSION
#define VERSION "1.0"
#endif

#define PIR_JITTER 60
#define MIN_DELAY 10

/* valori di default */
char *host = "127.0.0.1";
int port = 1883;
int keepalive = 60;
static char *heatmapFilePath = "/mnt/sd/Heatmap.kml";
static char *geomapFilePath = "/www/Geomap.txt";
unsigned int saveDelay = MIN_DELAY; /* pausa minima tra due salvataggi (in secondi) */
unsigned int pirJitter = PIR_JITTER; /* variazione segnale sensore PIR senza cambio stato */

extern KMLInfo kmlInfo;
extern LampData lampData[];
extern int numItems;

static volatile sig_atomic_t running = 1;

time_t timeLastSaved;
bool changed = true; /* arrivata misura nuova rispetto al KML salvato, se gia' creato */
bool writekml = true; /* scrive il file KML */

struct mosquitto *mosq = NULL;
int mosq_log_levels = MOSQ_LOG_ERR | MOSQ_LOG_INFO | MOSQ_LOG_WARNING;

/* topic */
char *axmj_in = "/axlight/f48e30d0-566f-4524-9130-1d65f17d8a53/stc/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/axmj/";
char *cmd_in = "/axmh/f48e30d0-566f-4524-9130-1d65f17d8a53/stc/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/cmdin/";
char *cmd_out = "/axmh/f48e30d0-566f-4524-9130-1d65f17d8a53/cts/nde/8ee098b5-c24b-43c3-9bf3-869a4302adef/cmdout/";

/** Macro per i messaggi diagnostici */
#define DEBUG
#ifdef DEBUG
#define Log(...) printf(__VA_ARGS__)
#else
#define Log(...) /* NOP */
#endif

/**
 * Funzione di confronto tra LampData, usata da qsort()
 * @author Flavio
 */
static int comp(const void * elem1, const void * elem2) {
	char *f = ((LampData*) elem1)->macaddr;
	char *s = ((LampData*) elem2)->macaddr;
	return strcmp(f, s);
}

/**
 * Converte una stringa in uppercase
 * @param s
 */
void strup(char * s) {
	while ((*s = toupper(*s))) {
		s++;
	}
}

/**
 * Verifica se il saveDelay ha un valore ammesso (1 sec.. 1 ora)
 * @param i
 * @return
 */
static bool validateSaveDelay(unsigned int i) {
	return (i > 0 && i < 3600);
}

/**
 * Parsa una stringa come int
 * @param str Stringa da convertire
 * @return Valore intero
 */
unsigned int parseAsInteger(char * str) {
	int val;
	if (sscanf(str, "%u", &val) == 1) {
		return val;
	}
	fprintf(stderr, "Formato inatteso del parametro: %s\n", str);
	return 0;
}

/**
 * Legge il file Geomap con le anagrafiche delle lampade di interesse
 * Ordina per mac address se necessario
 * @author Flavio
 */
void LoadGeomapFile(const char *fpath) {
	FILE *fp;
	if ((fp = fopen(fpath, "r")) != NULL) {
		char * lastmac = lampData[0].macaddr;
		int n;
		bool ordered = true;
		for (n = 0; n != MAX_LAMPS && fscanf(fp, "%*6[^;];%35[^;];%16[^;];%15[^;];%15[^;];%15[^|]|", lampData[n].nome, lampData[n].macaddr, lampData[n].coord1, lampData[n].coord2, lampData[n].coord3) == 5; n++) {
			strup(lampData[n].macaddr);
			lampData[n].pw1[0] = '\0';
			Log("[debug] %s %s\n", lampData[n].macaddr, lampData[n].nome);
			/* controlla se mac address in ordine alfanumerico crescente */
			if (strcmp(lastmac, lampData[n].macaddr) > 0) {
				ordered = false;
			}
			lastmac = lampData[n].macaddr;
			lampData[n].pir = 0;
			lampData[n].pir_change_count = 0;
			lampData[n].pir_delta_sign = 1;
		}
		numItems = n;
		fclose(fp);
		if (!ordered) {
			qsort(lampData, numItems, sizeof(*lampData), comp);
			Log("Voci Geomap riordinate per mac address crescente\n");
		}
	} else {
		err(EXIT_FAILURE, "File Geomap mancante: %s", fpath);
	}
}

/**
 * Ricerca binaria mediante puntatori per trovare un mac address (fonte: K&R)
 * @author Flavio
 *
 */
LampData *binsearch_macaddr(const char *macaddr, LampData *lamp, int n) {
	int cond;
	LampData *low = &lamp[0];
	LampData *high = &lamp[n];
	LampData *mid;

	int skip = 0;
	while (low < high) {
		mid = low + (high - low) / 2;
		if (strlen(macaddr) == 6) {
			/* ricerca solo suffisso macaddr */
			skip = 10;
		}
		if ((cond = strcmp(macaddr, mid->macaddr + skip)) < 0) {
			Log("[debug] %s < %s\n", macaddr, mid->macaddr + skip);
			high = mid;
		} else if (cond > 0) {
			Log("[debug] %s > %s\n", macaddr, mid->macaddr + skip);
			low = mid + 1;
		} else {
			Log("[debug] trovo: %s\n", mid->macaddr);
			return mid;
		}
	}
	return NULL;
}

/**
 * Estrapola il mac address di ogni misura ricevuta, se presente tra le lampade di interesse aggiorna LampData
 * @author Flavio
 */
void ReadDimmerFromMQTTMessage(char data[]) {
	char macaddr[17], power[4];
	int pir;
	int n;
	const char *pch = data;
	/* ignora preambolo, legge MAC, salta 17 campi, legge PW1 e PIR; n = caratteri letti */
	/* NOTA: aggiunto pipe (|) per accettare messaggi axmj fuori specifica */
	while (sscanf(pch, "%*[#.!|]NMEAS;MAC%16[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];PW1%[^;];%*[^;];%*[^;];%*[^;];AD0%5d;%*[^#!|]%n", macaddr, power, &pir,
			&n) == 3) {
		strup(macaddr);
		Log("[debug] Cerco %s\n", macaddr);
		LampData *lamp = binsearch_macaddr(macaddr, lampData, numItems);
		if (lamp != NULL) {
			Log("[debug] PW1 di %s = '%s'(era '%s') \n", macaddr, power, lamp->pw1);
			if (strcmp(lamp->pw1, power) != 0) { /* aggiorna solo se diverso */
				strcpy(lamp->pw1, power);
				changed = true; /* va ricreato il file KML */
			}

			Log("[debug] PIR di %s = '%d' (era '%d')\n", macaddr, pir, lamp->pir);
			int pir0 = lamp->pir;
			lamp->pir = pir;
			if (lamp->pir_delta_sign * (pir - pir0) > pirJitter) { /* variazione PIR sopra soglia */
				Log("[debug] PIR di %s fa scattare contatore: delta %d\n", macaddr, pir - pir0);
				lamp->pir_change_count++;
				lamp->pir_delta_sign = -lamp->pir_delta_sign;
				changed = true; /* va ricreato il file KML */
			}
		}
		pch += n;
	}
}

/**
 * Callback per gestire i messaggi di log di mosquitto
 * @author Flavio
 */
static void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str) {
	/* Visualizza messaggi di log secondo flag in mosq_log_levels */
	if (level & mosq_log_levels) {
		puts(str);
	}
}

/**
 * Callback all'atto della connessione, effettua l'iscrizione ai topic
 * @author Flavio
 */
static void my_connect_callback(struct mosquitto *mosq, void *userdata, int rc) {
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
 * @author Flavio
 */
static void my_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
	int i;
	printf("Subscribe mid %d; qos %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++) {
		printf(", %d", granted_qos[i]);
	}
	puts("\n");
	fflush(stdout);
}

/**
 * Callback per i messaggi ricevuti da mosquitto
 * @author Flavio
 */
static void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	if (message->payload != NULL) {

		char *payload = (char *) message->payload;
		char *pch = message->topic + 91; /* confronto solo parte finale stringa usando aritmetica dei puntatori */
		char aResp[50];
		char *resp = NULL;

		if (strcmp(pch, axmj_in + 91) == 0) {
			Log("[debug] Messaggio da axmj <= %s\n", payload);
			ReadDimmerFromMQTTMessage(payload);
		} else if (strcmp(pch, cmd_in + 91) == 0) {
			Log("[debug] Messaggio da cmdin <= %s\n", payload);
			/* todo split */
			if (strncmp("OFF", payload, 4) == 0) {
				writekml = false;
				fputs("\nGenerazione kml sospesa\n", stdout);
				resp = "OK\r\n";
			} else if (strncmp("ON", payload, 2) == 0) {
				writekml = true;
				fputs("\nGenerazione kml attiva\n", stdout);
				resp = "OK\r\n";
				char * sVal = strchr(payload, ' ');
				if (sVal != NULL) {
					unsigned int val = parseAsInteger(++sVal);
					if (validateSaveDelay(val)) {
						saveDelay = val;
						printf("KML ricreato ogni %u secondi (se vi sono valori nuovi)\n", val);
					} else {
						resp = "ERR\r\n";
					}
				}
			} else if (strncmp("SETCOUNT", payload, 8) == 0) {
				LampData *lamp;
				unsigned int v = 0;
				char mac[17]; /* todo globale? */
				int n = sscanf(payload, "SETCOUNT %u %16s", &v, mac);
				if (n == 2) {
					strup(mac);
					lamp = binsearch_macaddr(mac, lampData, numItems);
					if (lamp != NULL) {
						lamp->pir_change_count = v;
						printf("Conteggio PIR di %s = %d\n", mac, v);
						resp = "OK\r\n";
					} else {
						resp = "NOTFOUND\r\n";
					}
				} else if (n == 1) {
					for (lamp = lampData; lamp < lampData + numItems; lamp++) {
						lamp->pir_change_count = v;
					}
					printf("Conteggi PIR = %d\n", v);
					sprintf(aResp, "OK %u\r\n", numItems);
					resp = aResp;
				} else {
					resp = "ERR\r\n";
				}
			} else if (strncmp("GETSTATUS", payload, 10) == 0) {
				sprintf(aResp, (writekml ? "ON %u\r\n" : "OFF\r\n"), saveDelay);
				resp = aResp;
			} else if (strncmp("?", payload, 2) == 0) {
				resp = "OFF, ON, GETSTATUS, SETCOUNT valorecontatore macaddr";
			}
		} else {
			Log("[debug] Ignorato messaggio da %s <= %s\n", message->topic, payload);
		}
		if (resp != NULL) {
			Log("[debug] Risposta => %s", resp);
			mosquitto_publish(mosq, NULL, cmd_out, strlen(resp), resp, 0, false);
		}
	}
	fflush(stdout);
}

/**
 * Gestisce un segnale di interrupt. Deve contenere solo operazioni atomiche, per evitare deadlock
 * @author Flavio
 * @param signo
 */
static void sig_handler(int signo) {
	running = 0;
}

/**
 * Acquisisce eventuali parametri da riga di comando. Sintassi minimale:
 * un solo carattere, poi il separatore : (senza spazi) ed il valore.
 * Parametri non validi vengono ignorati, si usa il valore di default.
 * Non controlla esistenza parametri duplicati.
 * @param argc
 * @param argv
 * @author Flavio
 */
static void parseArguments(int argc, char *argv[]) {
	int i = argc;
	while (--i) {
		char * value = strchr(argv[i], ':') + 1;
		/* si aspetta un formato tipo "x:valore" */
		switch (argv[i][0]) {
		case '?':
			puts("Parametri: h(ost) p(ort) k(eepalive) i(nput_geomap) o(utput_heatmap) r(efresh_sec) j(itter_pir) l(og_levels)");
			puts("Tutti opzionali. Usare solo il carattere minuscolo iniziale, poi : ed il valore.");
			printf("Default: h:%s p:%d k:%d i:%s o:%s r:%d j:%d l:%d\n", host, port, keepalive, geomapFilePath, heatmapFilePath, saveDelay, pirJitter, mosq_log_levels);
			exit(EXIT_SUCCESS);
			break;
		case 'l':
			mosq_log_levels = parseAsInteger(value);
			break;
		case 'h':
			host = value;
			break;
		case 'i':
			geomapFilePath = value;
			break;
		case 'o':
			heatmapFilePath = value;
			break;
		case 'p':
			port = parseAsInteger(value);
			break;
		case 'k':
			keepalive = parseAsInteger(value);
			break;
		case 'j':
			pirJitter = parseAsInteger(value);
			break;
		case 'r':
			saveDelay = parseAsInteger(value);
			if (!validateSaveDelay(saveDelay)) {
				puts("Valore saveDelay non valido, uso default.\n");
				saveDelay = MIN_DELAY;
			}
			break;
		default:
			printf("Parametro sconosciuto: %s\n", argv[i]);
		}
	}
}

/* TEST */
/*
void main(int argc, char *argv[]) {
	// Evita problemi con stdout/stderr durante debug interattivo
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	char strMqttMsg[50000] =
			"#.#NMEAS;MAC00158D00010A4C57;IDN56;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9CAE;LQI78;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER5;PW0900;PW1900;PW2900;TMP29;VCC3214;AD01;AD11;AD22;AD320;MOS4;#!##.#NMEAS;MAC00158D00011B1387;IDN70;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B138B;LQI150;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3183;AD01;AD11;AD21;AD3599;MOS1;#!##.#NMEAS;MAC00158D000109EDE4;IDN113;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B132B;LQI138;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3193;AD01;AD11;AD21;AD3599;MOS2;#!##.#NMEAS;MAC00158D000109EA5D;IDN48;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B1384;LQI45;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER3;PW0900;PW1900;PW2900;TMP29;VCC3180;AD02;AD12;AD22;AD320;MOS2;#!##.#NMEAS;MAC00158D00011B1327;IDN41;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D00011B9C7D;LQI168;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3187;AD01;AD12;AD22;AD3599;MOS4;#!##.#NMEAS;MAC00158D00011B132B;IDN23;FWV0036;HMS18:09:00;DOY01;MTY2;PAR00158D000109EA5B;LQI171;PKS0;PKR0;PKL0;VAC0;IAC0;PAT0;PRE0;CEA255;CER4;PW0900;PW1900;PW2900;TMP29;VCC3180;AD01;AD11;AD21;AD318;MOS2;#!#";
	kmlInfo.folder = "Lumi";
	kmlInfo.name = "HeatmapTest";
	LoadGeomapFile(GeomapFilePath);
	ReadDimmerFromMQTTMessage(strMqttMsg);
	int i;
	for (i = 0; i < numItems; i++) {
		printf("%s mac=%s pw1=%s\n", lampData[i].nome, lampData[i].macaddr, lampData[i].pw1);
	}
}
*/

int main(int argc, char *argv[]) {
	parseArguments(argc, argv);
	puts("mh-"VERSION" avviato - parametro ? per opzioni");

	kmlInfo.folder = "luci";
	kmlInfo.name = "Heatmap";
	LoadGeomapFile(geomapFilePath);

	mosquitto_lib_init();

	if (!(mosq = mosquitto_new("MqttHeatmap", true/*clean_session*/, NULL))) {
		err(EXIT_FAILURE, "Mosquitto_new failed");
	}
	mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

	if (mosquitto_connect(mosq, host, port, keepalive) != MOSQ_ERR_SUCCESS) {
		err(EXIT_FAILURE, "Mosquitto_connect failed");
	}

	mosquitto_loop_start(mosq);

	/* abilita cattura SIGINT (ctrl+C) e SIGTERM (kill -p) */
	if (signal(SIGINT, sig_handler) == SIG_ERR) {
		puts("Cannot catch SIGINT ...");
	}
	if (signal(SIGTERM, sig_handler) == SIG_ERR) {
		puts("Cannot catch SIGTERM ...");
	}

	changed = true;
	timeLastSaved = 0;

	while (running) {
		/* se ci sono misure nuove e scrittura kml e' attiva e tempo da ultimo salvataggio superiore a intervallo minimo in secondi */
		if (changed && writekml && difftime(time(NULL), timeLastSaved) >= saveDelay) {
			WriteKMLFile(heatmapFilePath);
			timeLastSaved = time(NULL); /* istante attuale */
			changed = false;
		}
		sleep(saveDelay > MIN_DELAY ? MIN_DELAY : saveDelay); /* reagisce ai comandi ricevuti da cmd_in non oltre MIN_DELAY secondi */
	}
	mosquitto_disconnect(mosq);
	mosquitto_loop_stop(mosq, false);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	printf("Servizio %s interrotto.\n", argv[0]);
	return EXIT_SUCCESS;
}
