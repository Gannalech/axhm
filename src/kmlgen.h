/*
 * kmlgen.h
 *
 *  Created on: 27 ott 2017
 *      Author: Flavio Bucceri
 */

typedef struct {
	char *name;
	char *autore;
} KMLInfo;

typedef struct {
	char macaddr[17];
	char nome[36];
	char pw1[5];
	char pir[5];
	int pir_delta_sign;
	unsigned int pir_change_count;
	char coord1[16], coord2[16], coord3[16];
} LampData;

#define MAX_LAMPS 150

LampData lampData[MAX_LAMPS];
KMLInfo kmlInfo;
int numItems;

void write_kml(FILE *fp, KMLInfo *kml, LampData *item);
int WriteKMLFile(char* fname);

