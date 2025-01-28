/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "sendrecvUtil.h"
#include "pollLib.h"

#define MAXBUF 1500
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int mainServerSocket);
void addNewSocket(int mainServerSocket);
void processClient(int clientSocket);
void sendToClient(int socketNum, uint8_t *data_buff, int sendLen);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	serverControl(mainServerSocket);

	return 0;
}

void recvFromClient(int clientSocket)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF);

	if (messageLen > 0)
	{
		printf("Message received on socket %d, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer);
		sendToClient(clientSocket, dataBuffer, messageLen);
	}
	else
	{
		close(clientSocket);
		removeFromPollSet(clientSocket);
		printf("Connection closed by other side\n");
	}
}

void sendToClient(int socketNum, uint8_t *data_buff, int sendLen) {
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	if (sendLen > MAXBUF - 3) {
		fprintf(stderr, "Send buffer is too large\n");
		exit(-1);
	}
	
	sent =  sendPDU(socketNum, data_buff, sendLen);
	if (sent < 0) {
		perror("send call");
		exit(-1);
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

void serverControl(int mainServerSocket) {
	int socketNumber = 0;
	// Initialize poll set
	setupPollSet();
	addToPollSet(mainServerSocket);

	// Control loop
	for (;;) {
		socketNumber = pollCall(-1);
		if (socketNumber == mainServerSocket) {
			addNewSocket(mainServerSocket);
		} else {
			processClient(socketNumber);
		}
	}
}

void addNewSocket(int mainServerSocket) {
	// wait for client to connect
	int clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

	// Add client socket to poll set
	addToPollSet(clientSocket);
}

void processClient(int clientSocket) {
	// Receive PDU
	recvFromClient(clientSocket);
}


