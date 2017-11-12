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
 * Genera un xml nel formato KML
 */
#include <stdio.h>
#include "kmlgen.h"
#include "xmlwriter.h"
#include <string.h>

static char* PLACEMARK = "Placemark";
static char* ID = "id";
static char* COORDINATES = "coordinates";
static char* POINT = "Point";
static char* DIMMER = "Dimmer";
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
	aTagA(fp, "xmlns", "http://earth.google.com/kml/2.0");
	aTagA(fp, "xmlns:atom", "http://www.w3.org/2005/Atom");
	aTag(fp, "Document");

	/* HEADER */
	aTag(fp, NAME);
	aText(fp, kml->name);
	cTag(fp);
	aTag(fp, "atom:author");
	aText(fp, kml->autore);
	cTag(fp);
	aTag(fp, "atom:name");
	cTag(fp);
	/*aTag(fp,"atom:link"); aTagA(fp,"href","http://www.allix.it"); cTag(fp);*/
	aTag(fp, FOLDER);
	aTag(fp, NAME);
	cTag(fp);

	LampData it;

	/* VALORI LAMPADA */
	for (i = 0; i < numItems; i++) {
		it = item[i];
		if (strlen(it.pw1) != 0) {
			aTag(fp, PLACEMARK);
			aTagA(fp, ID, it.macaddr);
			aTag(fp, NAME);
			aText(fp, it.nome);
			cTag(fp);
			aTag(fp, DIMMER);
			aText(fp, it.pw1);
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
 LampData lampData[] = { { "00158D0000EB5ECE", "nomelamp1", 0, "1.1", "2.0", "0" }, { "00158D0000EB5ECE", "nomelamp2", 50, "1", "2", "3" }, { "00158D0000EB3EF2", "nomelamp3", 1000, "1", "2", "3" } };
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
