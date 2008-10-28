/*
 * structures.h
 *
 *  Created on: 2008-10-25
 *      Author: chriss
 */

#ifndef STRUCTURES_H_
#define STRUCTURES_H_
/*
 * empty -> wolna jednostka
 * new -> swiezo polaczony klient
 * working -> w trakcie wymiany komunikatow
 * stopped -> zakonczyl wymiane komunikatow, mozna sprawdzic status zakonczenia
 * 			  i zamknac socketa
 */

#define CI_EMPTY 0
#define CI_NEW 1
#define CI_WORKING 2
#define CI_STOPPED 3

typedef struct clientinfo{

	int status;//stan

	int sockd;//deskryptor socketa
	int procid;//id procesu komunikujacego sie z klientem
	struct sockaddr_in clientData;//informacje o kliencie


}cokolwiek;//aby warninga 'uniknac useless storage class specifier in empty declaration' ^^

#endif /* STRUCTURES_H_ */
