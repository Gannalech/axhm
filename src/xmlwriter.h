/*
 * xmlwriter.h
 *
 *  Created on : 25 ott 2017
 *      Author : Flavio Bucceri
 * XML writer incrementale
 */

#ifndef XMLWRITER_H_
#define XMLWRITER_H_

/****************************************************************************
 *
 * Scrive un nuovo tag su stream e lo salva sullo stack lifo. Se supera la
 * dimensione statica dello stack mette un tag diagnostico di errore nell'XML generato.
 * Occorre allora incrementare MAX_LIFO_INDEX e ricompilare.
 *
 ****************************************************************************/
void aTag(FILE *fp, char * name);

/****************************************************************************
 *
 * Scrive testo sullo stream, completa eventuali tag precedenti
 *
 ****************************************************************************/
void aText(FILE *fp, char * text);

/****************************************************************************
 *
 * Chiude il tag corrente. Se non definito mette un tag diagnostico di errore nell'XML generato
 *
 ****************************************************************************/
void cTag(FILE *fp);

/****************************************************************************
 *
 * Chiude tag aperti fino al livello (n). Livello iniziale e' 0.
 *
 ****************************************************************************/
void cTags(FILE *fp, int n);
/****************************************************************************
 *
 * Aggiunge un attributo al tag. Se usata in un contesto errato mette un tag diagnostico di errore nell'XML generato
 *
 ****************************************************************************/
void aTagA(FILE *fp, char * name, char * value);

/* Funzioni di comodo che chiamano le funzioni dell' xmlwriter */
/****************************************************************************
 *
 * Crea un tag con un singolo attributo.
 *
 ****************************************************************************/
void write_element_with_attribute(FILE *fp, char *ename, char *etext, char *aname, char *avalue);

/****************************************************************************
 *
 * Crea un tag senza attributo.
 *
 ****************************************************************************/
void write_element(FILE *fp, char *name, char *text);
/****************************************************************************
 *
 * Chiude il tag attualmente aperto. Se non aperto mette un tag
 * diagnostico di errore nell'XML generato
 *
 ****************************************************************************/
void write_end_element(FILE *fp);

#endif /* XMLWRITER_H_ */
