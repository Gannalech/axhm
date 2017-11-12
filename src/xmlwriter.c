/*
 * XML Writer
 *
 *   Creazione : 23 ott 2017
 *      Autore : Flavio Bucceri
 *    Versione : 1.1  (27 ott 2017)
 */

/**
 * @file xmlwriter.c
 * @author Flavio Bucceri
 * @date 27 ott 2017
 * Scrive su stream codice XML generato in modo incrementale.
 * Tipico ordine di chiamata delle funzioni: aTag, aTagA (opzionale), cText (opzionale), cTag.
 * In caso di errore nell'uso, genera un tag commento nell'XML a scopo diagnostico.
 * (Le "funzioni ausiliarie" non permettono piu' di un attributo.)
 */

/* Includes */
#include <stdio.h>

/* Macro definitions */
#define FALSE 0
#define TRUE 1

/* Stack LIFO */
#define MAX_LIFO_INDEX 10
static char *tags[MAX_LIFO_INDEX + 1];
static unsigned int top = 0; /* livello stack */
static int isEmpty = TRUE; /* nodo vuoto */

/**
 * Scrive un nuovo tag su stream e lo salva sullo stack lifo.
 * Se supera la dimensione statica dello stack mette un tag diagnostico nell'XML generato.
 * In quel caso occorre incrementare MAX_LIFO_INDEX e ricompilare il codice.
 * @author Flavio
 **/
void aTag(FILE *fp, char * name) {
	if (isEmpty && top > 0) {
		putc('>', fp);
	}
	putc('<', fp);
	fputs(name, fp);
	if (top > MAX_LIFO_INDEX) {
		fputs("<!-- INCREASE-MAX-LIFO-INDEX -->", fp);
	} else {
		tags[top++] = name;
		isEmpty = TRUE;
	}
}

/**
 * Scrive testo sullo stream, finalizzando eventuali tag pendenti
 * @author Flavio
 *
 * XXX varargs per concatenare testo?
 **/
void aText(FILE *fp, char * text) {
	if (isEmpty && top > 0) {
		isEmpty = FALSE;
		putc('>', fp);
	}
	fputs(text, fp);
}

/**
 * Chiude il tag corrente.
 * Scrive un tag diagnostico sullo stream se usata in un contesto errato.
 * @author Flavio
 **/
void cTag(FILE *fp) {
	if (top > 0) {
		top--;
		if (isEmpty) {
			putc('/', fp);
		} else {
			putc('<', fp);
			putc('/', fp);
			fputs(tags[top], fp);
		}
		putc('>', fp);
		isEmpty = FALSE;
	} else {
		fputs("<!-- CLOSED-UNOPENED-TAG -->", fp);
	}
}

/**
 * Chiude i tag aperti fino al livello (k). Livello iniziale 0, nodo radice ha livello 1.
 * @author Flavio
 **/
void cTags(FILE *fp, int k) {
	while (top > k) {
		cTag(fp);
	}
	if (top != 0)
		isEmpty = TRUE;
}

/**
 * Aggiunge un attributo al tag di apertura.
 * Scrive un tag diagnostico sullo stream se usata in un contesto errato.
 * @author Flavio
 **/
void aTagA(FILE *fp, char * name, char * value) {
	if (!isEmpty) {
		fprintf(fp, "<!-- MISPLACED-ATTRIBUTE-LEVEL-%d:", top);
	}
	fprintf(fp, " %s=\"%s\"", name, value);
	if (!isEmpty) {
		fputs(" -->", fp);
	}
}

/**
 * Crea un tag con singolo attributo (funzione ausiliaria).
 * @author Flavio
 **/
void write_element_with_attribute(FILE *fp, char *ename, char *etext, char *aname, char *avalue) {
	aTag(fp, ename);
	aTagA(fp, aname, avalue);
	if (etext) {
		aText(fp, etext);
	}
}

/**
 * Crea un tag senza attributo (funzione ausiliaria).
 * @author Flavio
 **/
void write_element(FILE *fp, char *name, char *text) {
	aTag(fp, name);
	if (text) {
		aText(fp, text);
	}
}

/**
 * Chiude il tag attualmente aperto. Se non aperto scrive un tag
 * diagnostico di errore nell'XML generato
 * @author Flavio
 **/
void write_end_element(FILE *fp) {
	cTag(fp);
}

/* INIZIO CODICE DI TEST */
/*
 static void test_xml_1(FILE *fp) {
 write_element_with_attribute(fp, "news", "test", "attrib", "value");
 write_element(fp, "name", NULL);
 write_element_with_attribute(fp, "news", NULL, "attrib", "value");
 write_element(fp, "name", "text");
 write_end_element(fp);
 write_end_element(fp);
 write_end_element(fp);
 write_end_element(fp);
 }

 static void test_xml_2(FILE *fp) {
 int nItems = 15;
 aText(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"); // preambolo
 aTag(fp, "kml");
 aTagA(fp, "xmlns", "http://earth.google.com/kml/2.0");
 aTagA(fp, "xmlns:atom", "http://www.w3.org/2005/Atom");
 aTag(fp, "Document");
 aTag(fp, "name");
 aText(fp, "test");
 cTag(fp);
 //aTagA(fp,"misplaced_attrib", "?????");
 aTag(fp, "atom:author");
 cTag(fp);
 aTag(fp, "atom:name");
 cTag(fp);
 aTag(fp, "atom:link");
 aTagA(fp, "href", "http://www.allix.it");
 cTag(fp);
 aTag(fp, "Folder");
 aTag(fp, "name");
 cTag(fp);
 for (int i = 0; i < nItems; i++) { // 5 test results
 aTag(fp, "Placemark");
 aTagA(fp, "id", "00158D0000EB5ECE");
 aTag(fp, "name");
 cTag(fp);
 aTag(fp, "dimming");
 aText(fp, "5.0");
 cTag(fp);
 aTag(fp, "Point");
 aTag(fp, "coordinates");
 aText(fp, "-74.006393,40.714172,0");
 cTag(fp);
 cTag(fp);
 cTag(fp);
 }
 cTags(fp, 0);
 aText(fp, "????");
 aTag(fp, "name");
 cTags(fp, 1);
 aText(fp, "????");
 cTag(fp);
 cTag(fp);
 cTag(fp);
 }

 int main(void) {
 // print some xml to stdout
 fputs("Metodo 1:\n\n",stdout);
 test_xml_1(stdout);
 fputs("\n\nMetodo 2:\n\n",stdout);
 test_xml_2(stdout);
 }
 */
/* FINE CODICE DI TEST */
