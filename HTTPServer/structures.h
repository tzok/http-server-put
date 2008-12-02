/*
 * structures.h
 *
 *  Created on: 2008-10-25
 *      Author: chriss, tzok
 */
#ifndef structures_h
#define structures_h

enum Bool {
	false,
	true
};

/* possible client status */
enum ClientStatus {
	empty, /// server has a free unit to process client
	new, /// client that has just connected
	working, /// already exchanging messages
	finished
/// finished exchanging messages
};

/* possible server status */
enum ServerStatus {
	running, stopped
};

/*!
 * Structure to store information about each connected client
 */
typedef struct ClientInfo {
	enum ClientStatus status; /// connection status
	int sockd; /// socket descriptor
	int procid; /// id of a process that communicates with client
	struct sockaddr_in clientData; /// client address information
} ClientInfo;

#endif /* structures_h */
