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
	int ad_bias[2];
	char adc_coord1[2][21], adc_coord2[2][21];
} LampData;

#define MAX_LAMPS 150

extern KMLInfo kmlInfo;
extern LampData lampData[];
extern int numItems;

void write_kml(FILE *fp, KMLInfo *kml, LampData *item);
int WriteKMLFile(char* fname);

