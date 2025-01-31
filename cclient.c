/******************************************************************************
* myClient.c
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

#define MAXBUF 1400
#define DEBUG_FLAG 1

void sendToServer(int socketNum, uint8_t flag, uint8_t *buff);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl(int clientSocket);
void processStdin(int clientSocket);
void processMsgFromServer(int clientSocket);
uint8_t unicastMessage(uint8_t *buff, uint8_t *sendBuff);

// Globals
char clientName[101];
uint8_t clientLength = 0;


int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	// set up the TCP Client socket
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	// Send client handle to server
	sendToServer(socketNum, 1, (uint8_t *)argv[1]);

	clientControl(socketNum);
		
	return 0;
}

void sendToServer(int socketNum, uint8_t flag, uint8_t *buff)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */

	switch (flag) {
		case 1: {
			/* Send client handle to server */
			uint8_t handleLength = strlen((char *)buff) + 1;
			sendLen = handleLength + 1;
			clientLength = handleLength - 1;
			memcpy(clientName, buff, handleLength);
			memcpy(sendBuf, &handleLength, 1);
			memcpy(sendBuf + 1, buff, handleLength);
		}
		case 4: {
			/* Broadcast */
			
			break;
		}
		case 5:
			/* Unicast Message */
			sendLen = unicastMessage(buff, sendBuf);
			break;

		case 6:
			/* Multicast */
			break;

		case 10:
			/* Request List of Handles */
			break;
		
		default: 
			printf("Invalid Command\n");
			printf("$: ");
			fflush(stdout);
			return;
	}
	
	sent = sendPDU(socketNum, sendBuf, sendLen, &flag);
	if (sent < 0) {
		perror("send call");
		exit(-1);
	}

	printf("\tAmount of data sent is: %d\n", sent);
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';

	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s client-handle host-name port-number \n", argv[0]);
		exit(1);
	}
}

void clientControl(int clientSocket) {
	int socketNumber = 0;
	// Initialize poll set
	setupPollSet();
	addToPollSet(clientSocket);
	addToPollSet(STDIN_FILENO);

	// Control loop
	for (;;) {
		socketNumber = pollCall(-1);
		if (socketNumber == STDIN_FILENO) {
			processStdin(clientSocket);
		} else if (socketNumber == clientSocket) {
			processMsgFromServer(socketNumber);
		}
	}
}

void processStdin(int clientSocket) {
	uint8_t buff[MAXBUF], buffcp[MAXBUF];   
	int sendLen = readFromStdin(buff);
	memcpy(buffcp, buff, MAXBUF);
	printf("\tRead: %s string len: %d (including null)\n", buff, sendLen);

	// Extract command using strtok
    char *command = strtok((char *)(buffcp), " ");
    if (!command) {
        printf("Invalid input\n");
        return;
    }

    // Route command to the appropriate function
	uint8_t flag = 0;
    if (strcmp(command, "%M") == 0 || strcmp(command, "%m") == 0) {
		flag = 5;
    // } else if (strcmp(command, "%C") == 0) {
    //     processCommandC(buff);
    // } else if (strcmp(command, "%B") == 0) {
    //     processCommandB(buff);
    // } else if (strcmp(command, "%L") == 0) {
    //     processCommandL(buff);
    } else {
        printf("Unknown command: %s\n", command);
    }

	printf("BUFF: %s\n", buff + 3);

	sendToServer(clientSocket, flag, buff + 3);
}

void processMsgFromServer(int socketNum) {
	uint8_t recvBuf[MAXBUF];
    int recvLen = 0;

	uint8_t flag;

    // Receive data from the server
    recvLen = recvPDU(socketNum, recvBuf, MAXBUF, &flag);
    if (recvLen == 0) {
        // Server has terminated the connection
        printf("\nServer has terminated\n");
        close(socketNum);
        exit(0);
    } else if (recvLen < 0) {
        perror("recv call2");
        close(socketNum);
        exit(-1);
    }

	switch (flag) {
		case 2: {
			/* Handle good */
			printf("$: ");
			fflush(stdout);
			return;
		}
		
		case 3: {
			/* Handle Taken */
			printf("Handle already in use: %s\n", (char *)recvBuf);
        	exit(0);
		}

		case 7:
			/* code */
			break;
		
		case 11:
			/* code */
			break;

		case 12:
			/* code */
			break;
		
		case 13:
			/* code */
			break;
		
		default:
			break;
	}

	printf("$: ");
	fflush(stdout);
}

uint8_t unicastMessage(uint8_t *buff, uint8_t *sendBuff) {
    // Extract the client handle
    char *handle = strtok((char *)buff, " ");
    if (!handle) {
        printf("Invalid input format for %%M\n");
        return -1;
    }

    uint8_t length = strlen(handle);
    if (length > 100) {
        printf("Invalid handle: too long\n");
        return -1;
    }
	printf("Here1\n");

    // Extract the remaining message
	char *message = (char *)(buff + length + 1);
	uint8_t messageLen = strlen(message) + 1;

    uint8_t numHandles = 1;

    // Construct the send buffer
	printf("%d%s%d%d%s%s\n", clientLength, clientName, numHandles, length, handle, message);
    memcpy(sendBuff, &clientLength, 1);
    memcpy(sendBuff + 1, clientName, clientLength);
    memcpy(sendBuff + clientLength + 1, &numHandles, 1);
    memcpy(sendBuff + clientLength + 2, &length, 1);
	memcpy(sendBuff + clientLength + 3, handle, length);
    memcpy(sendBuff + clientLength + length + 3, message, messageLen);

    printf("Unicast Message Prepared: Handle=%s, Message=%s\n", handle, message);

	return clientLength + length + messageLen + 3;
}


