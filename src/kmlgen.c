/*
 * KML Generator
 *
 *  Created on: 25 ott 2017
 *      Author: Flavio Bucceri
 *     Version: 1.1  (27 ott 2017)
 *
 *  Genera un xml nel formato KML
 */
/**
 * @file kmlgen.c
 * @author Flavio Bucceri
 * @date 27 ott 2017
 * Genera un file KML 2.2 visualizzabile ad esempio in Google Earth
 */
#include <stdio.h>
#include "kmlgen.h"
#include "xmlwriter.h"
#include <string.h>

static char* DOCUMENT = "Document";
static char* EXTENDED_DATA = "ExtendedData";
static char* DATA = "Data";
static char* VALUE = "value";
static char* PLACEMARK = "Placemark";
static char* ID = "id";
static char* COORDINATES = "coordinates";
static char* POINT = "Point";
static char* FOLDER = "Folder";
static char* NAME = "name";
static char* KML = "kml";

extern KMLInfo kmlInfo;
extern LampData lampData[];
extern int numItems;

static int i;

/**
 * Scrive su stream il codice KML
 * @author Flavio
 * @param fp stream su cui scrivere
 * @param kml dati per il preambolo
 * @param item array dei dati delle lampade
 */
void write_kml(FILE* fp, KMLInfo* kml, LampData *item) {
	/* PREAMBOLO */
	TAG(KML);
	ATTR("xmlns", "http://www.opengis.net/kml/2.2");
	TAG(DOCUMENT);

	/* HEADER */
	TAG(NAME);
	TEXT(kml->name);
	CTAG();
	TAG(FOLDER);
	TAG(NAME);
	TEXT(kml->folder);
	CTAG();

	char str[10]; /* fino a 9 cifre? */

	/* VALORI */
	for (i = 0; i < numItems; i++) {
		if (strlen(item->pw1) != 0) {
			TAG(PLACEMARK);
			ATTR(ID, item->macaddr);
			TAG(NAME);
			TEXT(item->nome);
			CTAG();

			TAG(EXTENDED_DATA);
			TAG(DATA);
			ATTR(NAME, "MAC");
			TAG(VALUE);
			TEXT(item->macaddr);
			CTAG();
			CTAG();
			TAG(DATA);
			ATTR(NAME, "PW1");
			TAG(VALUE);
			TEXT(item->pw1);
			CTAG();
			CTAG();
			TAG(DATA);
			/*ATTR(NAME, "AD0-#");
			 TAG(VALUE);
			 sprintf(str, "%d", it->adc_change_count);
			 TEXT(str);*/
			ATTR(NAME, "PIR:Count");
			TAG(VALUE);
			sprintf(str, "%d", item->pir_count);
			TEXT(str);
			CTAG();
			CTAG();
			CTAG();

			TAG(POINT);
			TAG(COORDINATES);
			TEXT(item->coord1);
			TEXT(",");
			TEXT(item->coord2);
			TEXT(",");
			TEXT(item->coord3);
			CTAG();
			CTAG();
			CTAG();
		}
		item++; // lampada successiva
	}
	/* Chiusura tag aperti */
	CTAGS(0);
}

/**
 * Scrive su disco il file KML
 * @author Flavio
 * @param fname Nome del file completo di path
 * @return Esito operazione
 */
int WriteKMLFile(char* fname) {
	FILE *fp = fopen(fname, "w");
	if (fp == NULL) {
		fprintf(stderr, "Error opening file: %s\n", fname);
		return 1;
	}
	write_kml(fp, &kmlInfo, lampData);
	fclose(fp);
	return 0;
}

/* INIZIO CODICE DI TEST */
/*
 // Parametro opzionale a riga di comando: il nome del file kml
 int main(int argc, char* argv[]) {
 // Autoflush Eclipse CPP buffered output to see it during debug (workaround)
 setvbuf(stdout, NULL, _IONBF, 0);
 //setvbuf(stderr, NULL, _IONBF, 0);

 KMLInfo kmlInfo = { "Heatmap", "MH" };
 numItems = 3;
 LampData lampData[] = { { "00158D0000EB5ECE", "nomelamp1", "0", "2", +1, 0, "1.1", "2.0", "0" }, { "00158D0000EB5ECE", "nomelamp2", "50", "2", +1, 0, "1", "2", "3" }, { "00158D0000EB3EF2", "nomelamp3", "1000", "2", +1, 0, "1", "2", "3" } };
 write_kml(stdout, &kmlInfo, lampData);

 // default if filename not specified at command line
 char* fname = (argc == 2 ? argv[1] : "heatmap_kml.xml");

 FILE *fp = fopen(fname, "w");
 if (fp == NULL) {
 fprintf(stderr, "Error opening file: %s\n", fname);
 return 1;
 }

 write_kml(fp, &kmlInfo, lampData);
 fclose(fp);
 return 0;
 }
 */
/* FINE CODICE DI TEST */
