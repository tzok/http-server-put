/*
 * server.c
 *
 *  Created on: 2008-10-24
 *      Author: chriss, tzok
 */
#include "headers.h"
#include "structures.h"
#include "pages.h"
#include "prototypes.h"
#include "mime.h"

/* server configuration */
const int serverPort = 6666;
const int queueSize = 20;
const int maxConnections = 20;

/* timeouts */
const int serverTimeout = 5;
const int clientTimeout = 3;

/* other constants */
const int maxCommandLength = 128;

/* possible server status codes */
enum codes {
	ok,
	created,
	accepted,
	noContent,
	movedPermanently,
	movedTemporarily,
	notModified,
	badRequest,
	unauthorized,
	forbidden,
	notFound,
	internalServerError,
	notImplemented,
	badGateway,
	serviceUnavailable
};
const char *statusCode[] = { "200 OK", "201 Created", "202 Accepted",
		"204 No Content", "301 Moved Permanently", "302 Moved Temporarily",
		"304 Not Modified", "400 Bad Request", "401 Unauthorized",
		"403 Forbidden", "404 Not Found", "500 Internal Server Error",
		"501 Not Implemented", "502 Bad Gateway", "503 Service Unavailable" };

/* server response header which is sent to every request */
const char *serverHeader = "Server: http-server-put\n"
	"Content-Length: %d\n"
	"Content-Type: %s\n"
	"\n";

/* prototypes of functions used */
inline void assert(int, const char*);

/*!
 * Get a list of headers from a socket
 * @param sockd Input socket
 * @return Pointer to a list of headers
 */
struct bstrList* getRequest(int sockd) {
	char buf[1024];
	struct bstrList *requestList;
	bstring line;
	bstring all = bfromcstr("");
	int i = 0;

	/* read incoming bytes until one empty line is found */
	while (1) {
		do {
			read(sockd, &buf[i++], 1);
		} while (buf[i - 1] != '\n');

		if (i < 3)
			break;

		buf[i] = 0x0;

		line = bfromcstr(buf);
		bconcat(all, line);
		bdestroy(line);
		i = 0;
	}

	requestList = bsplit(all, '\n');
	bdestroy(all);

	return requestList;
}

/*!
 * Creates a buffer with correct HTTP/1.0 response
 * @param status Status code of given operation
 * @param contentType Literal containing one of possible MIME types
 * @param entitySize Size of entity body
 * @param entity Pointer to entity content
 */
char* makeResponseBody(enum codes status, const char *contentType,
		int entitySize, char *entity, int *responseSize) {
	/* status line */
	char statusLine[64];
	int statusSize = sprintf(statusLine, "HTTP/1.0 %s\n", statusCode[status]);

	/* line with date */
	char dateLine[40];
	struct tm current;
	now(&current);
	int dateSize = dateToStr(dateLine, &current);

	/* rest of headers including content-length */
	char headerLines[256];
	int headersSize = sprintf(headerLines, serverHeader, entitySize,
			contentType);

	/* create response */
	char *response = (char*) malloc(statusSize + dateSize + headersSize
			+ entitySize);
	int i, j = 0;
	for (i = 0; i < statusSize; ++i)
		response[j++] = statusLine[i];
	for (i = 0; i < dateSize; ++i)
		response[j++] = dateLine[i];
	for (i = 0; i < headersSize; ++i)
		response[j++] = headerLines[i];
	for (i = 0; i < entitySize; ++i)
		response[j++] = entity[i];
	*responseSize = j;
	return response;
}

/*!
 * Creates a response to GET method
 */
char* createResponse(struct bstrList *requestList, int *responseSize) {
	struct bstrList *currentLine;
	char *response;

	currentLine = bsplit(requestList->entry[0], ' ');

	if (biseqcstr(currentLine->entry[0], "GET")) {
		bdelete(currentLine->entry[1], 0, 1); // to remove unnecessary "/" character
		int fd = openat(AT_FDCWD, (const char*) bdata(currentLine->entry[1]), O_RDONLY);
		if (fd < 0)
			response = makeResponseBody(notFound, "text/html; charset=utf-8",
					strlen(notFoundPage), (char*) notFoundPage, responseSize);
		else {
			/* distinguish file type from extension */
			int i, j, k;
			char *contentType = "application/octet-stream";
			char *data = (char*) bdata(currentLine->entry[1]);
			for (i = currentLine->entry[1]->slen - 1; i >= 0; --i)
				if (data[i] == '.')
					break;
			if (i > 0) {
				char extension[currentLine->entry[1]->slen - i];
				for (k = 0, j = i + 1; j < currentLine->entry[1]->slen; ++j, ++k)
					extension[k] = tolower(data[j]);
				extension[k] = 0;
				printf("%s\n", extension);
				for (j = 0; j < mimeTypeCount; ++j)
					if (!(strcmp(extension, mimeExtensions[j])))
						break;
				contentType = mimeTypes[j];
			}

			int size = lseek(fd, 0, SEEK_END);
			char *buffer = (char*) malloc(size);
			lseek(fd, SEEK_SET, 0);
			read(fd, buffer, size);
			response = makeResponseBody(ok, contentType, size, buffer,
					responseSize);

		}
	} else if (biseqcstr(currentLine->entry[0], "POST")) {

	} else if (biseqcstr(currentLine->entry[0], "HEAD")) {

	} else
		response = makeResponseBody(notImplemented, "text/html; charset=utf-8",
				strlen(notImplementedPage), (char*) notImplementedPage,
				responseSize);
	bstrListDestroy(currentLine);
	return response;
}
/**
 * main()
 */
int main(int argc, char* argv[]) {
	/* shared memory block to store server state */
	int shmId = shmget(10, sizeof(int), 0666 | IPC_CREAT);
	assert(shmId != -1, "Couldn't create shared memory buffer\n");

	/* fork here, one process to handle I/O, one to process networking */
	int childId = fork();
	assert(childId >= 0, "Couldn't fork to create child process\n");

	/* *************************************************************************
	 * I/O process */
	if (childId) {
		char *serverState = (char*) shmat(shmId, 0, 0);
		while (1) {
			char command[maxCommandLength];
			scanf("%s", command);

			/* "stop" command */
			if (!strcmp(command, "stop")) {
				printf("Stopping server... please wait\n");
				*serverState = stopped;

				int stopRes;
				waitpid(childId, &stopRes, 0);

				if (stopRes)
					printf("There were some errors while stopping the server\n");
				else
					printf("Server stopped successfully\n");
				fflush(stdout);

				shmdt(serverState);
				shmctl(shmId, IPC_RMID,0);
				break;
			} else
				printf("Unknown command\n");
		}
		return 0;
	}
	/* ********************************************************************** */
	char *serverState = (char*) shmat(shmId, 0, 0);
	*serverState = running;

	/* reserve table for information about connected clients */
	int infoId = shmget(100, sizeof(struct ClientInfo) * maxConnections, 0666
			| IPC_CREAT);
	assert(infoId != -1, "Couldn't create shared memory buffer\n");

	/* initialize client array */
	struct ClientInfo *clients;
	clients = (struct ClientInfo*) shmat(infoId, 0, 0);
	int i;
	for (i = 0; i < maxConnections; i++)
		clients[i].status = empty;

	/* prepare server socket */
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	int serverSocket = socket(PF_INET,SOCK_STREAM, 0);
	assert(serverSocket != -1, "Couldn't create socket\n");

	/* let the system reuse this socket right after its closing */
	int optval;
	setsockopt(serverSocket, SOL_SOCKET,SO_REUSEADDR, &optval, sizeof(optval));

	/* bind the socket */
	int bindStatus = bind(serverSocket, (const struct sockaddr *) &serverAddr,
			sizeof(serverAddr));
	assert(bindStatus != -1, "Binding of the socket failed\n");

	/* start listening on the socket */
	int listenStatus = listen(serverSocket, queueSize);
	assert(listenStatus != -1, "Listening on the socket failed\n");

	int maxSD = serverSocket + 1;
	fd_set fsServer;
	FD_ZERO(&fsServer);

	struct timeval timeout;
	/* prepare server timeout */
	timeout.tv_sec = serverTimeout;
	timeout.tv_usec = 0;
	/* *************************************************************************
	 * networking process */
	int clientNumber;
	while (*serverState == running) {
		/* prepare server timeout */

		/* select client to connect to */
		FD_SET(serverSocket, &fsServer);
		int foundStatus = select(maxSD + 1, &fsServer, (fd_set*) 0,
				(fd_set*) 0, &timeout);

		/* count active clients; clean info about stopped clients */
		if (foundStatus < 0)
			printf("Select error\n");
		else if (!foundStatus) {
			/* reset server timeout */
			timeout.tv_sec = serverTimeout;
			timeout.tv_usec = 0;

			clientNumber = 0;
			for (i = 0; i < maxConnections; i++) {
				if (clients[i].status == working)
					++clientNumber;
				else if (clients[i].status == stopped) {
					clients[i].status = empty;
					waitpid(clients[i].procid, 0, WNOHANG);
					close(clients[i].sockd);
				}
			}
			printf("Connected clients: %d\n", clientNumber);
		}

		/* process new connection */
		if (FD_ISSET(serverSocket, &fsServer)) {
			struct sockaddr_in clientAddr;
			int size;
			int clientSocket = accept(serverSocket,
					(struct sockaddr*) &clientAddr, (socklen_t*) &size);
			assert(clientSocket != -1,
					"Couldn't create a connection's socket.\n");

			if (clientSocket > maxSD)
				maxSD = clientSocket;

			/* max number of connections reached */
			if (clientNumber == maxConnections) {
				printf("Too many connections\n");
				close(clientSocket);
				continue;
			}
			clientNumber++;

			/* add this new connection to array */
			for (i = 0; i < maxConnections; i++)
				if (clients[i].status == empty)
					break;
			clients[i].status = new;
			clients[i].sockd = clientSocket;
			memcpy(&clients[i].clientData, &clientAddr, size);

			/* fork here to create process communicating with new client */
			int pid = fork();
			if (!pid) {
				struct ClientInfo *myInfo;
				myInfo = shmat(infoId, 0, 0);
				myInfo[i].status = working;

				fd_set fsClient;
				FD_ZERO(&fsClient);

				/* *************************************************************/
				/* communication process */
				int exitRes = 0;
				while (myInfo[i].status == working) {
					FD_SET(clientSocket, &fsClient);

					struct timeval tout;
					tout.tv_sec = clientTimeout;
					tout.tv_usec = 0;

					int selectRes = select(clientSocket + 1, &fsClient,
							(fd_set*) 0, (fd_set*) 0, &tout);

					if (selectRes < 0) {
						exitRes = 1;
						break;
					} else if (!selectRes) {
					}

					if (FD_ISSET(clientSocket, &fsClient)) {
						/* read the socket */
						struct bstrList *tempList = getRequest(clientSocket);
						int responseSize;
						char *response =
								createResponse(tempList, &responseSize);
						write(clientSocket, response, responseSize);
						free(response);
					}
				}
				/* ************************************************************/

				FD_ZERO(&fsClient);
				myInfo[i].status = stopped;
				shmdt(myInfo);
				exit(exitRes);
			}
			clients[i].procid = pid;
		}
	}
	/* ************************************************************************/

	/* cleaning client data */
	if (clientNumber > 0) {
		for (i = 0; i < maxConnections; i++)
			clients[i].status = finished;
		sleep(clientTimeout + 1);
		for (i = 0; i < maxConnections; i++)
			close(clients[i].sockd);
	}

	/* close socket and free shared memory */
	close(serverSocket);
	shmdt(clients);
	shmdt(serverState);
	shmctl(infoId, IPC_RMID,0);

	return 0;
}

/**
 *Ensures some conditions are met. Otherwise, exits the application with error message
 * @param expr boolean expression to check
 * @param msg message to display if condition is not met
 */
inline void assert(int expr, const char *msg) {
	if (!expr) {
		printf(msg);
		exit(1);
	}
}
