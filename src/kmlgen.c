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

static int i, k;

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

	char str[50];

	/* VALORI */
	for (i = 0; i < numItems; i++) {
		for (k = 0; k < 2; k++) {
			if (item->ad[k] != -1) {
				TAG(PLACEMARK);
				snprintf(str, sizeof(str), "%s_AD%d", item->macaddr, k);
				ATTR(ID, str);
				TAG(NAME);
				snprintf(str, sizeof(str), "%s - Sensore %d", item->nome, k);
				TEXT(str);
				CTAG();
				TAG(EXTENDED_DATA);
				TAG(DATA);
				ATTR(NAME, "MAC");
				TAG(VALUE);
				TEXT(item->macaddr);
				CTAG();
				CTAG();
				TAG(DATA);
				ATTR(NAME, "ADC");
				TAG(VALUE);
				snprintf(str, sizeof(str), "%d", item->ad[k]);
				TEXT(str);
				CTAG();
				CTAG();
				CTAG();

				TAG(POINT);
				TAG(COORDINATES);
				TEXT(item->adc_coord1[k]);
				TEXT(",");
				TEXT(item->adc_coord2[k]);
				CTAG();
				CTAG();
				CTAG();
			}
		} // sensore successivo
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
