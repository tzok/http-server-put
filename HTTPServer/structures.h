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

enum HTTPVersion {
	http_0_9,
	http_1_0,
	http_1_1
};

/* possible client status */
enum ClientStatus {
	empty, /// server has a free unit to process client
	new, /// client that has just connected
	working, /// already exchanging messages
	finished
/// finished exchanging messages
};

/* realms for authentication */
typedef struct Realm {
	char name[256];
	char login[256];
	char pass[256];
	char uri[256][256];
	int count;
} Realm;

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
