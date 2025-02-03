#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "safeUtil.h"
#include <string.h>
#include <sys/socket.h>

#include "sendrecvUtil.h"

#define MAXBUF 1500

int sendPDU(int clientSocket, uint8_t *dataBuffer, int lengthOfData, uint8_t *flag) {
    // Ensure data fits in ethernet frame
    if (lengthOfData > 1497) {
        fprintf(stderr, "Send data is too large to fit in ethernet frame\n");
        return -1;
    }
    
    // Calculate PDU Size
    uint16_t pduSize = lengthOfData + 3;
    uint16_t pduSize_n = htons(pduSize);

    // Create PDU with max ethernet frame size
    uint8_t pduBuffer[1500];
    memcpy(pduBuffer, &pduSize_n, 2);
    memcpy(pduBuffer + 2, flag, 1);

    if (lengthOfData > 0) {
        memcpy(pduBuffer + 3, dataBuffer, lengthOfData);
    }

    // Send PDU
    int bytesSent = safeSend(clientSocket, pduBuffer, pduSize, 0);
    
    return bytesSent - 3;
}

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize, uint8_t *flag) {
    struct pduHeader {
        uint16_t pduSize_n;
        uint8_t flag;
    };

    uint16_t pduSize;

    struct pduHeader header;
    
    // Get PDU Size
    int bytesRecv = safeRecv(socketNumber, (uint8_t *)(&header), 3, MSG_WAITALL);

    pduSize = ntohs(header.pduSize_n);
    *flag = header.flag;

    if (pduSize - 3 == 0) {
        return pduSize;
    }

    // Check buffer size
    if (pduSize - 3 > bufferSize) {
        fprintf(stderr, "Buffer is too small for receive. PDU Size: %d\tBuffer Size: %d\n", pduSize, bufferSize);
        return -1;
    }

    // Get PDU Data
    bytesRecv = safeRecv(socketNumber, dataBuffer, pduSize - 3, MSG_WAITALL);

    return bytesRecv;
}

void safeSendPDU(int socketNum, uint8_t *data_buff, int sendLen, uint8_t *flag) {
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


