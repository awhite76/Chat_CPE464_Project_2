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
#include "handleTable.h"

#define MAXBUF 1500
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void serverControl(int mainServerSocket);
void addNewSocket(int mainServerSocket);
void processClient(int clientSocket);
void newClient(uint8_t *buff, int msgLen, int socket);
void rerouteMessage(int srcSocket, uint8_t *buff, int lengthOfData, uint8_t *flag_p);
void sendHandleList(int clientSocket);
void broadcastMessage(uint8_t *buff, int messageLen);
void badDstHandle(int srcSocket, char *handle);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	
	// Initialize handle table
	initTable();

	serverControl(mainServerSocket);

	return 0;
}

void recvFromClient(int clientSocket) {
	uint8_t dataBuffer[MAXBUF];
	uint8_t flag;
	int messageLen = 0;
	
	//now get the data from the client_socket
	messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF, &flag);

	if (messageLen <= 0) {
		close(clientSocket);
		removeFromPollSet(clientSocket);
		char *handleToRemove = lookUpBysocket(clientSocket);
		if (handleToRemove) {
			removeHandle(handleToRemove);
		}
		printf("Connection closed by other side\n");
		return;
	}

	printf("Message received on socket %d, length: %d, Data: %s, Flag: %d\n", clientSocket, messageLen, dataBuffer, flag);

	switch (flag) {
		case 1: {
			/* Send client handle to server */
			newClient(dataBuffer, messageLen, clientSocket);
			break;
		}
		case 4: {
			/* Broadcast */
			broadcastMessage(dataBuffer, messageLen);
			break;
		}
		case 5:
			/* Unicast Message (Same as flag 6) */

		case 6:
			/* Multicast */
			rerouteMessage(clientSocket, dataBuffer, messageLen, &flag);
			break;

		case 10:
			/* Request List of Handles */
			sendHandleList(clientSocket);
			break;
		
		default:
			fprintf(stderr, "Improper flag number: %d\n", flag);
			return;
			break;
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

void newClient(uint8_t *buff, int msgLen, int socket) {
	uint8_t handleSize;
	uint8_t handle[101];
	
	// Extract from buffer
	memcpy(&handleSize, buff, 1);
	memcpy(handle, buff + 1, handleSize);
	handle[handleSize] = '\0';

	// Check if valid handle
	if (strlen((char *)handle) == 0) {
		fprintf(stderr, "Can't set handle to empty string\n");
		return;
	}

	// Add handle to handle table
	int res = addHandle((char *)handle, socket);

	// Send response to client
	uint8_t flag;
	if (res == 0) {
		flag = 2;
		safeSendPDU(socket, NULL, 0, &flag);
	} else {
		flag = 3;
		safeSendPDU(socket, NULL, 0, &flag);
	}
}

void rerouteMessage(int srcSocket, uint8_t *buff, int lengthOfData, uint8_t *flag_p) {
	uint8_t srcHandle[100], dstHandle[101], srcLen, dstLen, dstNum;
	int socket = 0;

	// Extract application header
	memcpy(&srcLen, buff, 1);
	memcpy(srcHandle, buff + 1, srcLen);
	memcpy(&dstNum, buff + 1 + srcLen, 1);

	// Iteratively set destination
	int offset = 2 + srcLen;
	for (int i = 0; i < dstNum; i++) {
		memcpy(&dstLen, buff + offset, 1);
		memcpy(dstHandle, buff + offset + 1, dstLen);

		offset += 1 + dstLen;

		// Null terminate dst handle
		dstHandle[dstLen] = '\0';

		// Look up socket number of dst
		socket = lookUpHandle((char *)dstHandle);
		if (socket < 0) {
			fprintf(stderr, "Couldn't find handle: %s\n", dstHandle);
			badDstHandle(srcSocket, (char *)dstHandle);
			continue;
		}

		printf("Sending on socket: %d\n", socket);

		// Send packet
		safeSendPDU(socket, buff, lengthOfData, flag_p);
	}
}

void sendHandleList(int clientSocket) {
	uint8_t buff[MAXBUF];

	// Send number of handles in table
	uint8_t flag = 11;
	int list_population = getTablePopulation();
	int listPopulation_n = htonl(list_population);

	memcpy(buff, &listPopulation_n, 4);
	safeSendPDU(clientSocket, buff, 4, &flag);

	// Iteratively get handles and send to client
	char handle[100];
	uint8_t handleLen = 0;
	flag = 12;
	for (int i = 0; i < list_population; i++) {
		handleLen = getIthHandle(i + 1, handle);
		if (handleLen <= 0) {
			printf("Couldn't find %d handle\n", i + 1);
			break;
		}

		// Populate buffer for new message
		memcpy(buff, &handleLen, 1);
		memcpy(buff + 1, handle, handleLen);

		// Send packet with handle (flag 12)
		safeSendPDU(clientSocket, buff, handleLen + 1, &flag);
	}

	// Send done packet (flag 13)
	flag = 13;
	memset(buff, 0, MAXBUF);
	safeSendPDU(clientSocket, buff, 1, &flag);
}

void broadcastMessage(uint8_t *buff, int messageLen) {
	uint8_t flag = 4;

	// Parse sender handle
	uint8_t srcLen, srcHandle[101];
	memcpy(&srcLen, buff, 1);
	memcpy(srcHandle, buff + 1, srcLen);
	srcHandle[srcLen] = '\0';

	// Iteratively send to other clients
	int tablePopulation = getTablePopulation();
	uint8_t dstHandle[101], dstLen;
	int dstSocket = 0;
	for (int i = 0; i < tablePopulation; i++) {
		dstLen = getIthHandle(i + 1, (char *)dstHandle);
		if (dstLen <= 0) {
			printf("Couldn't find %d handle\n", i + 1);
			break;
		}

		// Dont send message back to sender
		if (strcmp((char *)srcHandle, (char *)dstHandle) == 0) {
			continue;
		}

		dstSocket = lookUpHandle((char *)dstHandle);
		if (dstSocket < 0) {
			fprintf(stderr, "Couldn't find handle: %s\n", dstHandle);
			continue;
		}

		// Send to destination
		safeSendPDU(dstSocket, buff, messageLen, &flag);
	}
}

void badDstHandle(int srcSocket, char *handle) {
	uint8_t buff[MAXBUF];
	uint8_t flag = 7;
	
	// Populate send buffer
	uint8_t handleLen = (uint8_t)strlen(handle);
	memcpy(buff, &handleLen, 1);
	memcpy(buff + 1, handle, handleLen);

	// Send back to client
	safeSendPDU(srcSocket, buff, 1 + handleLen, &flag);
}

