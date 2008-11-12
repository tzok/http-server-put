/*
 * prototypes.h
 *
 *  Created on: 2008-11-09
 *      Author: chriss, tzok
 */

#ifndef PROTOTYPES_H_
#define PROTOTYPES_H_

/* from time.c */

int compareDates(const struct tm *, const struct tm *);
int fileModDate(const char *, struct tm *);
void parseDate(const char *, struct tm *);
void dateToStr(char *, const struct tm *);
void now(struct tm *);

#endif /* PROTOTYPES_H_ */
