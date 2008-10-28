/*
 * server.c
 *
 *  Created on: 2008-10-24
 *      Author: chriss
 */
#include "headers.h"
#include "structures.h"

#define SERVER_PORT 6666
#define QUEUE_SIZE 20
#define MAX_CONNECTIONS 20

//timeout w selectach
#define SERVER_SELECT_TIME 5
#define CLIENT_SELECT_TIME 5



//stany serwera
#define SERVER_RUNNING 0x01
#define SERVER_STOP 0x02

int main(int argc, char *argv[]){





	//id dla stanu serwera
	int shmId = shmget(10,sizeof(int),0666|IPC_CREAT);

	if(shmId<0){
		printf("Nie mozna utworzyc segmentu pamieci wspoldzielonej\n");
		exit(1);
	}

	int childId= fork();

	if (childId<0){
		printf("Blad funkcji fork() nie mozna utworzyc procesu potomnego");
		exit(1);
	}


	if (childId){//proces I/O
		//stan serwera
		char *serverState = (char*)shmat(shmId,0,0);
		char command[20];
		int stopRes;
		while(1){

			scanf("%s",command);

			if(strcmp(command,"stop")==0){//zatrzymanie serwera
				printf("Zakanczanie pracy serwera... prosze czekac\n");
				*serverState &= ~SERVER_RUNNING;
				*serverState |= SERVER_STOP;



				waitpid(childId,&stopRes,0);

				if(stopRes)
					printf("Serwer nie zakonczyl pracy poprawnie\n");
				else
					printf("Serwer zakonczyl prace poprawnie\n");
				fflush(stdout);


				shmdt(serverState);
				shmctl(shmId,IPC_RMID,0);

				break;

			}
			else
				printf("Niznane polecenie...\n");

		}//while(1)
		return 0;

	}//I/O

	//pomocnicze
	int i;
	int optval=1;
	int pid;

	//diagnostyczne
	int bindStatus,listenStatus, foundStatus;


	//sockety i informacje
	int serverSocket, clientSocket;
	struct sockaddr_in serverAddr, clientAddr;

	//info o ilosci klientow i najwyzszym dekryptorze
	int clientNumber=0;
	int maxSD;//narazie w praktyce niepotrzebne - ale moze sie wykorzysta ;-)


	//do f. select()
	fd_set fsServer;
	struct timeval timeout;


	char *serverState = (char*)shmat(shmId,0,0);
	*serverState = SERVER_RUNNING;

	//id tablicy z informacjami o klientach
	int infoId = shmget(100,sizeof(struct clientinfo)*MAX_CONNECTIONS,0666|IPC_CREAT);

	if(infoId<0){
		printf("Nie mozna utworzyc segmentu pamieci wspoldzielonej\n");
		exit(1);
	}

	struct clientinfo * clients;
	clients = (struct clientinfo*)shmat(infoId,0,0);

	for (i=0;i<MAX_CONNECTIONS;i++)
		clients[i].status = CI_EMPTY;



	memset(&serverAddr, 0 ,sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	serverSocket = socket(PF_INET,SOCK_STREAM,0);
	if (serverSocket<0){
		printf("Nie mozna utworzyc socketa");
		exit(1);
	}

	//ponowne wyk socketa
	setsockopt(serverSocket,SOL_SOCKET,SO_REUSEADDR,&optval, sizeof(optval));

	bindStatus=bind(serverSocket,(const struct sockaddr *)&serverAddr,sizeof(serverAddr));
	if(bindStatus<0){
		printf( "Nie mozna zbindowac socketa");
		exit(1);
	}

	listenStatus = listen(serverSocket,QUEUE_SIZE);
	if(listenStatus<0){
		printf("Nie mozna ustawic kolejki");
		exit(1);
	}

	maxSD = serverSocket + 1;


	FD_ZERO(&fsServer);


	int size = sizeof(struct sockaddr);

	timeout.tv_sec = SERVER_SELECT_TIME;
	timeout.tv_usec = 0;


	while(*serverState & SERVER_RUNNING)
	   {


	       FD_SET(serverSocket, &fsServer);

	       foundStatus = select(maxSD + 1, &fsServer, (fd_set*)0, (fd_set*) 0, &timeout);

	       if (foundStatus < 0)
	       {
	               fprintf(stderr, "%s: Select error.\n", argv[0]);
	       }
	       if (foundStatus == 0)//zamykamykanie nieuzywanych socketow i zliczanie aktywnych polaczen
	       {
				   timeout.tv_sec = SERVER_SELECT_TIME;
	    	   	   timeout.tv_usec = 0;
				   clientNumber = 0;

				   for (i=0;i<MAX_CONNECTIONS;i++){
					   if(clients[i].status==CI_WORKING)
						   ++clientNumber;
					   else if(clients[i].status==CI_STOPPED){
						   clients[i].status = CI_EMPTY;
						   waitpid(clients[i].procid,0,WNOHANG);
						   close(clients[i].sockd);

					   }


				   }

				   printf("Podlaczonych klientow: %d\n", clientNumber);
	       }
	       if (FD_ISSET(serverSocket, &fsServer))//obsluga nowego polaczenia
	       {
	                clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, (socklen_t*)&size);
	                if (clientSocket < 0)
	                {
	                        fprintf(stderr, "%s: Can't create a connection's socket.\n", argv[0]);
	                        exit(1);
	                }



	                //if (clientSocket > maxSD) maxSD = clientSocket;


	                if (clientNumber==MAX_CONNECTIONS){
	                	printf("Za duzo polaczen;]");
	                	continue;
	                }

	                clientNumber++;
	                //wrzucamy polaczenie do tablicy
	                for (i=0;i<MAX_CONNECTIONS;i++)
	                	if(clients[i].status==CI_EMPTY)
	                		break;

	                clients[i].status = CI_NEW;
	                clients[i].sockd = clientSocket;
	                memcpy(&clients[i],&clientAddr,size);
	                pid =  fork();

	                if(!pid){//child
	                	char buf[50];
	                	struct clientinfo * myInfo;
	                	myInfo = shmat(infoId,0,0);

	                	struct timeval to;

	                	fd_set fsClient;
	                	FD_ZERO(&fsClient);

						int c;
						int selectRes;
						int exitRes=0;

						myInfo[i].status = CI_WORKING;

						while(myInfo[i].status == CI_WORKING){//child loop

							FD_SET(clientSocket,&fsClient);
							to.tv_sec = CLIENT_SELECT_TIME;
							to.tv_usec = 0;
							selectRes = select(clientSocket+1,&fsClient,(fd_set*)0,(fd_set*)0,&to);

							if(selectRes<0){//err
								exitRes = 1;

								break;
							}

							if(selectRes==0){//diagnostyka :]

							}

							if(FD_ISSET(clientSocket,&fsClient)){

								c = read(clientSocket,buf,50);

								if (buf[0]=='x'){
									break;
								}
								strcpy(buf, "Hello dude ;-D\n\0x0");
								write(clientSocket,buf, strlen(buf) );


							}


						}//child loop

						FD_ZERO(&fsClient);
						myInfo[i].status = CI_STOPPED;
						shmdt(myInfo);
						exit(exitRes);
	                }//if child

	                clients[i].procid = pid;

	       }



	   }//while


	//sprzatanie po procesach klientow
	if (clientNumber>0){
		for(i=0;i<MAX_CONNECTIONS;i++)
			clients[i].status = CI_STOPPED;

		sleep(CLIENT_SELECT_TIME+1);

		for(i=0;i<MAX_CONNECTIONS;i++)
			close(clients[i].sockd);
	}

	//zamykanie socketow i odlaczanie pamieci wspoldzielonej
	close(serverSocket);
	shmdt(clients);
	shmdt(serverState);

	shmctl(infoId,IPC_RMID,0);

	exit(0);
}
