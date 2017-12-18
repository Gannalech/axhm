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
 * Genera un xml nel formato KML 2.2 visualizzabile ad esempio in Google Earth
 */
#include <stdio.h>
#include "kmlgen.h"
#include "xmlwriter.h"
#include <string.h>

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
	aTag(fp, KML);
	aTagA(fp, "xmlns", "http://www.opengis.net/kml/2.2");
	aTag(fp, "Document");

	/* HEADER */
	aTag(fp, NAME);
	aText(fp, kml->name);
	cTag(fp);
	aTag(fp, FOLDER);
	aTag(fp, NAME);
	aText(fp, kml->folder);
	cTag(fp);

	LampData it;
	char str[10]; /* fino a 9 cifre? */

	/* VALORI */
	for (i = 0; i < numItems; i++) {
		it = item[i];
		if (strlen(it.pw1) != 0) {
			aTag(fp, PLACEMARK);
			aTagA(fp, ID, it.macaddr);
			aTag(fp, NAME);
			aText(fp, it.nome);
			cTag(fp);

			aTag(fp, EXTENDED_DATA);
			aTag(fp, DATA);
			aTagA(fp, NAME, "MAC");
			aTag(fp, VALUE);
			aText(fp, it.macaddr);
			cTag(fp);
			cTag(fp);
			aTag(fp, DATA);
			aTagA(fp, NAME, "Dimmer");
			aTag(fp, VALUE);
			aText(fp, it.pw1);
			cTag(fp);
			cTag(fp);
			aTag(fp, DATA);
			aTagA(fp, NAME, "PIR Count");
			sprintf(str, "%d", it.adc_change_count);
			aTag(fp, VALUE);
			aText(fp, str);
			cTag(fp);
			cTag(fp);
			cTag(fp);

			aTag(fp, POINT);
			aTag(fp, COORDINATES);
			aText(fp, it.coord1);
			aText(fp, ",");
			aText(fp, it.coord2);
			aText(fp, ",");
			aText(fp, it.coord3);
			cTag(fp);
			cTag(fp);
			cTag(fp);
		}
	}
	/* Chiusura tag aperti */
	cTags(fp, 0);
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
