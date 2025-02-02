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

#define MAXBUF 1401
#define DEBUG_FLAG 1

int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
void clientControl();
void processStdin();
void processMsgFromServer();
void sendHandle(char *handle);
void unicastMessage(uint8_t *buff);
void sendChunkByChunk(uint8_t *appHeader, int headerLen, char *msg, uint8_t *flag_p);
void recvMsg(uint8_t *buff);

// Globals
char clientName[100];
uint8_t clientLength = 0;
int clientSocket = 0;


int main(int argc, char * argv[])
{	
	checkArgs(argc, argv);

	// set up the TCP Client socket
	clientSocket = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	// Send client handle to server
	sendHandle(argv[1]);

	clientControl();
		
	return 0;
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

void clientControl() {
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
			processMsgFromServer();
		}
	}
}

void processStdin() {
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
    if (strcmp(command, "%M") == 0 || strcmp(command, "%m") == 0) {
		unicastMessage(buff);
    } else if (strcmp(command, "%C") == 0 || strcmp(command, "%c") == 0) {
        unicastMessage(buff);
    // } else if (strcmp(command, "%B") == 0) {
    //     processCommandB(buff);
    // } else if (strcmp(command, "%L") == 0) {
    //     processCommandL(buff);
    } else {
        printf("Unknown command: %s\n", command);
		printf("$: ");
		fflush(stdout);
    }
}

void processMsgFromServer() {
	uint8_t recvBuf[MAXBUF];
    int recvLen = 0;

	uint8_t flag;

    // Receive data from the server
    recvLen = recvPDU(clientSocket, recvBuf, MAXBUF, &flag);
    if (recvLen == 0) {
        // Server has terminated the connection
        printf("\nServer has terminated\n");
        close(clientSocket);
        exit(0);
    } else if (recvLen < 0) {
        perror("recv call2");
        close(clientSocket);
        exit(-1);
    }

	switch (flag) {
		case 2: {
			/* Handle good */
			break;
		}
		
		case 3: {
			/* Handle Taken */
			printf("Handle already in use: %s\n", (char *)recvBuf);
        	exit(0);
			break;
		}

		case 5: {
			/* Incoming Message */
			recvMsg(recvBuf);
			break;
		}

		case 6: {
			/* Incoming Message */
			recvMsg(recvBuf);
			break;
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

void unicastMessage(uint8_t *buff) {
	uint8_t flag = 0;
    // Extract the command 
    char *command = strtok((char *)buff, " ");
	if (strcmp(command, "%m") == 0 || strcmp(command, "%M") == 0) {
		flag = 5;
	} else if (strcmp(command, "%c") == 0 || strcmp(command, "%C") == 0) {
		flag = 6;
	}

	// Get number of destinations
	int numDst_int = 1;
	if (flag == 6) {
		numDst_int = atoi(strtok(NULL, " "));

		// Check number of destinations
		if (numDst_int > 9 || numDst_int < 2) {
			fprintf(stderr, "Invalid number of recipients: %d\n", numDst_int);
			return;
		}
	}

	uint8_t numDst = (uint8_t)numDst_int;
	printf("NumDst: %d\n", numDst);

	// Create app header
	int headerLength = 2 + clientLength;
	uint8_t header[MAXBUF];

	memcpy(header, &clientLength, 1);
	memcpy(header + 1, clientName, clientLength);
	memcpy(header + 1 + clientLength, &numDst, 1);

	// Extract the handles
	for (int i = 0; i < numDst; i++) {
		char *handle = strtok(NULL, " ");
		if (!handle) {
			printf("Invalid input format: Missing handle\n");
			return;
		}

		uint8_t length = strlen(handle);
		if (length > 100) {
			fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", handle);
			return;
		}

		// Add handle length and handle to header
		memcpy(header + headerLength, &length, 1);
		memcpy(header + headerLength + 1, handle, length);

		headerLength += 1 + length;
	}

	// Extract the remaining message
    char *message = strtok(NULL, ""); 
    if (!message) {
        message = ""; 
    }	

	// Send message in chunks
	sendChunkByChunk(header, headerLength, message, &flag);
}

void sendHandle(char *handle) {
	int handleLength = strlen(handle);
	// Check handle length
	if (handleLength > 100) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", handle);
		exit(-1);
	}

	// Populate send buffer
	uint8_t buff[101];

	uint8_t resizedLength = (uint8_t)handleLength;
	memcpy(buff, &resizedLength, 1);
	memcpy(buff + 1, handle, resizedLength);

	uint8_t flag = 1;

	// Store client handle globally
	clientLength = resizedLength;
	memcpy(clientName, handle, resizedLength);

	// Send packet
	sendPDU(clientSocket, buff, resizedLength + 1, &flag);
}

void sendChunkByChunk(uint8_t *appHeader, int headerLen, char *msg, uint8_t *flag_p) {
	uint8_t packetBuff[MAXBUF];
	int msgLen = strlen((char *)msg);
	int offset = 0;
	int chunkSize = 0;
	char nullChar = '\0';

	// Add header to packet buffer
	memcpy(packetBuff, appHeader, headerLen);

	while (offset < msgLen) {
		if (msgLen - 199 >= 0) {
			chunkSize = 199;
		} else {
			chunkSize = msgLen;
		}

		// Copy message chunk into packet buffer
		memcpy(packetBuff + headerLen, msg + offset, chunkSize);
		memcpy(packetBuff + headerLen + chunkSize, &nullChar, 1);

		sendPDU(clientSocket, packetBuff, headerLen + chunkSize + 1, flag_p);

		offset += chunkSize;
	}
}

void recvMsg(uint8_t *buff) {
	uint8_t srcHandle[101], srcLen, dstLen, dstNum;

	// Extract application header
	memcpy(&srcLen, buff, 1);
	memcpy(srcHandle, buff + 1, srcLen);
	memcpy(&dstNum, buff + 1 + srcLen, 1);

	// Null terminate dst handle
	srcHandle[srcLen] = '\0';

	// Get past destination handles
	int offset = 2 + srcLen;
	for (int i = 0; i < dstNum; i++) {
		memcpy(&dstLen, buff + offset, 1);

		offset += 1 + dstLen;
	}
	
	// Print sender and message
	printf("%s: %s\n", srcHandle, buff + offset);
}

