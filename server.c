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
void sendToClient(int socketNum, uint8_t *data_buff, int sendLen, uint8_t *flag);
void newClient(uint8_t *buff, int msgLen, int socket);
void rerouteMessage(uint8_t *buff, int lengthOfData, uint8_t *flag_p);

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
			
			break;
		}
		case 5:
			/* Unicast Message */
			rerouteMessage(dataBuffer, messageLen, &flag);
			break;

		case 6:
			/* Multicast */
			rerouteMessage(dataBuffer, messageLen, &flag);
			break;

		case 10:
			/* Request List of Handles */
			break;
		
		default:
			fprintf(stderr, "Improper flag number: %d\n", flag);
			return;
			break;
	}
}

void sendToClient(int socketNum, uint8_t *data_buff, int sendLen, uint8_t *flag) {
	int sent = 0;           
	
	if (sendLen > MAXBUF - 3) {
		fprintf(stderr, "Send buffer is too large\n");
		exit(-1);
	}
	
	sent = sendPDU(socketNum, data_buff, sendLen, flag);
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
		uint8_t *buff = (uint8_t *)"\0";
		sendToClient(socket, buff, 1, &flag);
	} else {
		flag = 3;
		sendToClient(socket, buff + 1, msgLen, &flag);
	}
}

void rerouteMessage(uint8_t *buff, int lengthOfData, uint8_t *flag_p) {
	uint8_t srcHandle[100], dstHandle[101], srcLen, dstLen, dstNum;
	int socket = 0;

	// Extract application header
	memcpy(&srcLen, buff, 1);
	memcpy(srcHandle, buff + 1, srcLen);
	memcpy(&dstNum, buff + 1 + srcLen, 1);

	// Iteratively set destination
	int offset = 2 + srcLen;
	printf("DstNum: %d\n", dstNum);
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
			// send back to client
		}

		printf("Sending on socket: %d\n", socket);

		// Send packet
		sendPDU(socket, buff, lengthOfData, flag_p);
	}
}
