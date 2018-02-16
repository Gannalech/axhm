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
 *	Genera periodicamente un heatmap in formato KML di lampade con anagrafica, potenza instantanea e conteggio sensore presenza (PIR).
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
#define VERSION "SNAPSHOT"
#endif

#define MIN_DELAY 10
#define MAC_SUFFIX_LEN 8

/* valori di default */
char *host = "127.0.0.1";
int port = 1883;
int keepalive = 60;
static char *heatmapFilePath = "/www/Heatmap/Scripts/Heatmap.kml";
static char *geomapFilePath = "/www/Geomap.txt";
unsigned int saveDelay = MIN_DELAY; /* pausa minima tra due salvataggi (in secondi) */

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
static bool validSaveDelay(unsigned int i) {
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
		for (n = 0;
				n != MAX_LAMPS
						&& fscanf(fp, "%*6[^;];%35[^;];%16[^;];%20[^;];%20[^;];%20[^;];%20[^|;]|", lampData[n].nome, lampData[n].macaddr, lampData[n].adc_coord1[0], lampData[n].adc_coord2[0], lampData[n].adc_coord1[1],
								lampData[n].adc_coord2[1]) == 6; n++) {
			strup(lampData[n].macaddr);
			lampData[n].ad[0] = -1;
			lampData[n].ad_bias[0] = 0;
			lampData[n].ad[1] = -1;
			lampData[n].ad_bias[1] = 0;
			Log("[debug] %s %s\n", lampData[n].macaddr, lampData[n].nome);
			/* controlla se mac address in ordine alfanumerico crescente */
			if (strcmp(lastmac, lampData[n].macaddr) > 0) {
				ordered = false;
			}
			lastmac = lampData[n].macaddr;
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
 * Modificato per accettare un suffisso di MAC_SUFFIX_LEN cifre invece del macaddress completo
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
		if (strlen(macaddr) == MAC_SUFFIX_LEN) {
			/* ricerca solo suffisso del macaddr */
			skip = 16 - MAC_SUFFIX_LEN;
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
void parseMeasures(char *data) {
	char macaddr[17];
	int ad[2];
	int n, k;
	const char *pch = data;
	/* ignora preambolo, legge MAC, salta piu' campi, legge AD0 e AD1; n = caratteri letti */
	/* NOTA: aggiunto pipe (|) per accettare messaggi axmj fuori specifica */
	while (sscanf(pch, "%*[#.!|]NMEAS;MAC%16[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];AD0%5d;AD1%5d;%*[^#!|]%n", macaddr, &ad[0],
			&ad[1], &n) == 3) {
		strup(macaddr);
		Log("[debug] Cerco %s\n", macaddr);
		LampData *lamp = binsearch_macaddr(macaddr, lampData, numItems);
		if (lamp != NULL) {
			for (k = 0; k < 2; k++) {
				Log("[debug] AD%d di %s = %d (era %d)\n", k, macaddr, ad[k], lamp->ad[k]);
				changed = (lamp->ad[k] != ad[k]); /* aggiorna solo se diverso */
				lamp->ad[k] = ad[k];
			}
		}
		pch += n;
	}
}

/**
 * Callback per gestire i messaggi di log di mosquitto
 * @author Flavio
 */
static void log_callback(struct mosquitto *mosq, void *obj, int level, const char *str) {
	/* Visualizza messaggi di log secondo flag in mosq_log_levels */
	if (level & mosq_log_levels) {
		puts(str);
	}
}

/**
 * Callback all'atto della connessione, effettua l'iscrizione ai topic
 * @author Flavio
 */
static void connect_callback(struct mosquitto *mosq, void *userdata, int rc) {
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
static void subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
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
static void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	if (message->payload != NULL) {

		char *payload = (char *) message->payload;
		char *pch = message->topic + 91; /* confronto solo parte finale nome topic usando aritmetica dei puntatori */
		char aResp[50];
		char *resp = NULL;

		if (strcmp(pch, axmj_in + 91) == 0) {
			Log("[debug] Messaggio da axmj <= %s\n", payload);
			parseMeasures(payload);
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
					if (validSaveDelay(val)) {
						saveDelay = val;
						printf("KML ricreato ogni %u secondi (se vi sono valori nuovi)\n", val);
					} else {
						resp = "ERR\r\n";
					}
				}
			} else if (strncmp("SET-", payload, 4) == 0) {
				LampData *lamp;
				int v;
				int k;
				char mac[17]; /* todo globale? */
				changed = false;
				int n = sscanf(payload, "SET-%d %u %16s", &k, &v, mac);
				if (n == 3) {
					strup(mac);
					lamp = binsearch_macaddr(mac, lampData, numItems);
					// simulo reset contatori lato mh usando ad_bias
					if (lamp != NULL) {
						if (lamp->ad[k] == -1) {
							resp = "NOVALUEYET\r\n";
						} else {
							changed |= (lamp->ad_bias[k] != v - lamp->ad[k]);
							lamp->ad_bias[k] = lamp->ad[k] - v;
							printf("AD%d (%s) = %d (bias %d)\n", k, mac, v, lamp->ad_bias[k]);
							resp = "OK\r\n";
						}
					} else {
						resp = "NOTFOUND\r\n";
					}
				} else if (n == 2) {
					// setta tutti i mac in geomap, quelli offline non appariranno in Heatmap
					for (lamp = lampData; lamp < lampData + numItems; lamp++) {
						changed |= (lamp->ad[k] - lamp->ad_bias[k] != v);
						lamp->ad_bias[k] = lamp->ad[k] - v;
					}
					printf("AD%d (tutti) = %d\n", k, v);
					sprintf(aResp, "OK %d\r\n", numItems);
					resp = aResp;
				} else {
					resp = "ERR\r\n";
				}
			} else if (strncmp("GETSTATUS", payload, 10) == 0) {
				sprintf(aResp, (writekml ? "ON %u\r\n" : "OFF\r\n"), saveDelay);
				resp = aResp;
			} else if (strncmp("?", payload, 2) == 0) {
				resp = "OFF\r\nON\r\nGETSTATUS\r\nSET-<indice> <valorecontatore> <macaddr>\r\n";
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
		/* legge il primo char di argv[i] */
		switch (argv[i][0]) {
		case '?':
			puts("mh-" VERSION);
			puts("Parametri: h(ost) p(ort) k(eepalive) i(nput_geomap) o(utput_heatmap) r(efresh_sec) v(erbosity)");
			puts("Tutti opzionali. Usare solo il carattere minuscolo iniziale, poi : ed il valore.");
			printf("Default: h:%s p:%d k:%d i:%s o:%s r:%d v:%d\n", host, port, keepalive, geomapFilePath, heatmapFilePath, saveDelay, mosq_log_levels);
			exit(EXIT_SUCCESS);
			break;
		case 'v':
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
		case 'r':
			saveDelay = parseAsInteger(value);
			if (!validSaveDelay(saveDelay)) {
				puts("Valore saveDelay non valido, uso default.\n");
				saveDelay = MIN_DELAY;
			}
			break;
		default:
			printf("Parametro sconosciuto: %s\n", argv[i]);
		}
	}
}

int main(int argc, char *argv[]) {
#ifdef DEBUG
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
#endif
	parseArguments(argc, argv);
	/* VERSION deve contenere apici, nel Makefile va scritto VERSION=\"1.0.1\" */
	puts("mh-" VERSION " avviato - parametro ? per opzioni");
	kmlInfo.folder = "misure";
	kmlInfo.name = "Heatmap";
	LoadGeomapFile(geomapFilePath);

	mosquitto_lib_init();

	if (!(mosq = mosquitto_new("MqttHeatmap", true/*clean_session*/, NULL))) {
		err(EXIT_FAILURE, "Mosquitto_new failed");
	}
	mosquitto_log_callback_set(mosq, log_callback);
	mosquitto_connect_callback_set(mosq, connect_callback);
	mosquitto_message_callback_set(mosq, message_callback);
	mosquitto_subscribe_callback_set(mosq, subscribe_callback);

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
	printf("%s interrotto.\n", argv[0]);
	return EXIT_SUCCESS;
}
