/**
 * @mainpage Our project http-server-put is an implementation of HTTP/1.0 server according to RFC 1945 document.
 * @author Tomasz Zok, Krzysztof Rosinski
 * @date 05-12-2008
 * @version 1.0
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
const int clientTimeout = 5;

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
char* createListPage(char*);
void decode(char*, char*);

/* global variables */
Realm realm[32];
int realmCount = 0;

/**
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
		while (read(sockd, &buf[i], 1) == 1)
			if (buf[i++] == '\n')
				break;

		if (i < 3)
			break;

		buf[i] = 0;
		line = bfromcstr(buf);
		bconcat(all, line);
		bdestroy(line);
		i = 0;
	}
	requestList = bsplit(all, '\n');
	bdestroy(all);
	return requestList;
}

/**
 * Creates a buffer with correct HTTP/1.0 response
 * @param[in] status Status code of given operation
 * @param[in] contentType Literal containing one of possible MIME types
 * @param[in] entitySize Size of entity body
 * @param[in] entity Pointer to entity content
 * @param[out] responseSize Will contain size of created response
 * @return Pointer to buffer containig full response
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

/**
 * Creates a response to GET method. This method analyzes incoming requests and responses appropriately
 * @param[in] requestList List of lines of full HTTP/1.x request
 * @param[out] responseSize Will contain size of created response
 * @return Full HTTP/1.0 response with requested URI or an error page
 */
char* createResponse(struct bstrList *requestList, int *responseSize) {
	bstring method = 0, uri = 0, version = 0;
	char *response;
	struct bstrList *currentLine = 0;

	/* read request line */
	currentLine = bsplit(requestList->entry[0], ' ');
	if (currentLine->qty != 3) {
		response = makeResponseBody(badRequest, "text/html; charset=utf-8",
				strlen(badRequestPage), (char*) badRequestPage, responseSize);
		goto ResponseCreated;
	}

	method = currentLine->entry[0];
	uri = currentLine->entry[1];
	version = currentLine->entry[2];

	/* filter empty requests */
	if (!method->slen || !uri->slen || !version->slen) {
		response = makeResponseBody(badRequest, "text/html; charset=utf-8",
				strlen(badRequestPage), (char*) badRequestPage, responseSize);
		goto ResponseCreated;
	}

	/* check http version */
	int httpVersion;
	if (!(strncmp((const char*) version->data, "HTTP/0.9", 8)))
		httpVersion = http_0_9;
	else if (!(strncmp((const char*) version->data, "HTTP/1.0", 8)))
		httpVersion = http_1_0;
	else if (!(strncmp((const char*) version->data, "HTTP/1.1", 8)))
		httpVersion = http_1_1;
	else {
		response = makeResponseBody(badRequest, "text/html; charset=utf-8",
				strlen(badRequestPage), (char*) badRequestPage, responseSize);
		goto ResponseCreated;
	}

	/* GET */
	int i, j, k;
	if (biseqcstr(method, "GET")) {
		/* check if authorization header was sent */
		for (i = 1; i < requestList->qty; ++i) {
			if (!(strncmp((char *) requestList->entry[i]->data,
					"Authorization:", 14)))
				break;
		}
		int isAuthenticated;
		char login[256], pass[256];
		memset(login, 0, sizeof(login));
		memset(pass, 0, sizeof(pass));
		if (i == requestList->qty)
			isAuthenticated = false;
		else {
			/* if client authorizes itself, decode base64 data */
			isAuthenticated = true;
			char dataEncoded[256], dataDecoded[256];
			memset(dataEncoded, 0, sizeof(dataEncoded));
			memset(dataDecoded, 0, sizeof(dataDecoded));
			for (j = strlen("Authorization: Basic "), k = 0; j
					< requestList->entry[i]->slen; ++j, ++k)
				dataEncoded[k] = requestList->entry[i]->data[j];
			dataEncoded[k] = 0;
			decode(dataEncoded, dataDecoded);

			/* split decoded input into login and pass */
			char *cur = dataDecoded;
			char *which = login;
			while (*cur) {
				if (*cur == ':')
					which = pass;
				else
					*(which++) = *cur;
				++cur;
			}
		}

		/* check if access is authenticated */
		for (i = 0; i < realmCount; ++i) {
			for (j = 0; j < realm[i].count; ++j)
				if (!(strcmp((char*) uri->data, realm[i].uri[j])))
					break;
			if (j != realm[i].count)
				break;
		}
		if (i != realmCount) {
			/* if access is authenticated, yet no authorization from client
			 * send 401 Unathorized */
			if (!isAuthenticated) {
				char additionalHeader[256];
				sprintf(
						additionalHeader,
						"text/html; charset=utf-8\nWWW-Authenticate: Basic realm=\"%s\"",
						realm[i].name);
				response = makeResponseBody(unauthorized, additionalHeader,
						strlen(unauthorizedPage), (char*) unauthorizedPage,
						responseSize);
				goto ResponseCreated;
				/* if access is authentitaced, yet authorization fails
				 * send 403 Forbidden */
			} else if (strcmp(login, realm[i].login) || strcmp(pass,
					realm[i].pass)) {
				response = makeResponseBody(forbidden,
						"text/html; charset=utf-8", strlen(forbiddenPage),
						(char*) forbiddenPage, responseSize);
				goto ResponseCreated;
			}
		}

		/* request for root directory */
		if (blength(uri) == 1 && uri->data[0] == '/') {
			char *listPage = createListPage("");
			response = makeResponseBody(ok, "text/html; charset=utf-8", strlen(
					listPage), listPage, responseSize);
			free(listPage);
			goto ResponseCreated;
		}

		/* check if resource exists */
		int fd = openat(AT_FDCWD, (const char*) &uri->data[1], O_RDONLY);
		if (fd < 0) {
			response = makeResponseBody(notFound, "text/html; charset=utf-8",
					strlen(notFoundPage), (char*) notFoundPage, responseSize);
			goto ResponseCreated;
		}

		/* check if it's a directory or file */
		struct stat attrib;
		fstatat(AT_FDCWD, (const char*) &uri->data[1], &attrib, 0);

		/* if directory, then list its content */
		if (S_ISDIR(attrib.st_mode)) {
			/* if URI does not end with'/', then redirect */
			if (uri->data[uri->slen - 1] != '/') {
				char additionalHeader[128];
				sprintf(
						additionalHeader,
						"text/html; charset=utf-8\nLocation: http://localhost:6666%s/",
						uri->data);
				response = makeResponseBody(movedPermanently, additionalHeader,
						0, 0, responseSize);
				goto ResponseCreated;
			}
			char *listPage = createListPage((char*) uri->data);
			response = makeResponseBody(ok, "text/html; charset=utf-8", strlen(
					listPage), listPage, responseSize);
			free(listPage);
			goto ResponseCreated;
		}

		/* distinguish mime type from extension */
		char *contentType = "application/octet-stream";
		char *data = (char*) uri->data;
		for (i = currentLine->entry[1]->slen - 1; i >= 0; --i)
			if (data[i] == '.')
				break;
		if (i > 0) {
			char extension[uri->slen - i];
			for (k = 0, j = i + 1; j < currentLine->entry[1]->slen; ++j, ++k)
				extension[k] = tolower(data[j]);
			extension[k] = 0;
			for (j = 0; j < mimeTypeCount; ++j)
				if (!(strcmp(extension, mimeExtensions[j])))
					break;
			contentType = mimeTypes[j];
		}

		/* read requested URI and create a response out of it */
		int size = lseek(fd, 0, SEEK_END);
		char *buffer = (char*) malloc(size);
		lseek(fd, SEEK_SET, 0);
		read(fd, buffer, size);
		response
				= makeResponseBody(ok, contentType, size, buffer, responseSize);
		free(buffer);
		goto ResponseCreated;

		/* POST */
	} else if (biseqcstr(currentLine->entry[0], "POST")) {

		/* HEAD */
	} else if (biseqcstr(currentLine->entry[0], "HEAD")) {

	} else
		return makeResponseBody(notImplemented, "text/html; charset=utf-8",
				strlen(notImplementedPage), (char*) notImplementedPage,
				responseSize);

	/* clean up all the structures used */
	ResponseCreated: if (method)
		bdestroy(method);
	if (uri)
		bdestroy(uri);
	if (version)
		bdestroy(version);
	if (currentLine)
		bstrListDestroy(currentLine);

	return response;
}

/**
 * Creates listing of a directory as a HTML page
 * @param path Path to a directory
 * @return HTML showing directory listing
 */
char* createListPage(char *path) {
	/* create absolute path to directory */
	char buffer[256];
	getcwd(buffer, sizeof(buffer));
	strcat(buffer, path);

	/* list files in it */
	struct dirent **namelist;
	int count = scandir(buffer, &namelist, 0, alphasort);

	/* prepare final page */
	const char* start = "<html>\n"
		"	<head>\n"
		"		<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\n"
		"		<meta name=\"Author\" content=\"Tomasz Zok, Krzystof Rosinski\"/>\n"
		"	</head>\n"
		"	\n"
		"	<body>\n";
	const char* element = "	<a href='%s'>%s</a></br>\n";
	const char
			* end =
					"	<br/>\n	Server: http-server-put<br/>Software written by: Tomasz Zok, Krzysztof Rosinski</br>\n"
						"	</body>\n"
						"</html>\n";
	char *page = (char*) malloc(strlen(start) + strlen(end) + count * 512);
	strcpy(page, start);

	/* add files and subfolders to this page */
	int i;
	for (i = 0; i < count; ++i) {
		if ((strcmp(namelist[i]->d_name, ".")) && (strcmp(namelist[i]->d_name,
				".."))) {
			char file[256];
			sprintf(file, element, namelist[i]->d_name, namelist[i]->d_name);
			strcat(page, file);
		}
		free(namelist[i]);
	}
	free(namelist);
	strcat(page, end);
	return page;
}

/*
 * main()
 */
int main(int argc, char* argv[]) {
	/* parse configuration file */
	FILE *file = fopen("config", "r");
	if (file) {
		char line[256];
		int isRealm = false;
		while (!feof(file)) {
			if (!fgets(line, sizeof(line), file))
				break;
			int i;
			/* line starting new realm is like: [%s] */
			if (line[0] == '[' && line[strlen(line) - 1 == ']']) {
				for (i = 1; i < strlen(line) - 2; ++i)
					realm[realmCount].name[i - 1] = line[i];
				realm[realmCount].name[i - 1] = 0;
				isRealm = true;
				realm[realmCount].count = 0;
			/* line with login is like: login=%s */
			} else if (isRealm && !(strncmp(line, "login=", 6))) {
				for (i = 6; i < strlen(line) - 1; ++i)
					realm[realmCount].login[i - 6] = line[i];
				realm[realmCount].login[i - 6] = 0;
			/* line with password is like: pass=%s */
			} else if (isRealm && !(strncmp(line, "pass=", 5))) {
				for (i = 5; i < strlen(line) - 1; ++i)
					realm[realmCount].pass[i - 5] = line[i];
				realm[realmCount].pass[i - 5] = 0;
			/* lines with URIs are like: uri=%s */
			} else if (isRealm && !(strncmp(line, "uri=", 4))) {
				for (i = 4; i < strlen(line) - 1; ++i)
					realm[realmCount].uri[realm[realmCount].count][i - 4]
							= line[i];
				realm[realmCount].uri[realm[realmCount].count++][i - 4] = 0;
			/* empty line finishes one realm configuration */
			} else if (line[0] == '\n') {
				++realmCount;
				isRealm = false;
			}
		}
		++realmCount;
		fclose(file);
	}

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
	int optval = 0;
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
	int clientNumber = 0;
	while (*serverState == running) {
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
			int size = 0;
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
						break;
					}

					if (FD_ISSET(clientSocket, &fsClient)) {
						/* read the socket */
						struct bstrList *tempList = getRequest(clientSocket);
						int responseSize;
						char *response =
								createResponse(tempList, &responseSize);
						write(clientSocket, response, responseSize);
						free(response);
						bstrListDestroy(tempList);
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
 * Ensures some conditions are met. Otherwise, exits the application with error message
 * @param expr boolean expression to check
 * @param msg message to display if condition is not met
 */
inline void assert(int expr, const char *msg) {
	if (!expr) {
		printf(msg);
		exit(1);
	}
}
