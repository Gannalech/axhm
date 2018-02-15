/*
 * kmlgen.h
 *
 *  Created on: 27 ott 2017
 *      Author: Flavio Bucceri
 */

typedef struct {
	char *name;
	char *folder;
} KMLInfo;

typedef struct {
	char macaddr[17];
	char nome[36];
	int ad[2];
// XXX usa virgola per coordinate.
	char adc_coord1[2][21], adc_coord2[2][21];
} LampData;

#define MAX_LAMPS 150

LampData lampData[MAX_LAMPS];
KMLInfo kmlInfo;
int numItems;

void write_kml(FILE *fp, KMLInfo *kml, LampData *item);
int WriteKMLFile(char* fname);

