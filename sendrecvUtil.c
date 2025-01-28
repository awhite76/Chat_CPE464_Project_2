#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "safeUtil.h"
#include <string.h>
#include <sys/socket.h>

#include "sendrecvUtil.h"

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData) {
    // Ensure data fits in ethernet frame
    if (lengthOfData > 1498) {
        fprintf(stderr, "Send data is too large to fit in ethernet frame\n");
        exit(-1);
    }
    
    // Calculate PDU Size
    uint16_t pduSize = lengthOfData + 2;
    uint16_t pduSize_n = htons(pduSize);

    // Create PDU with max ethernet frame size
    uint8_t pduBuffer[1500];
    memcpy(pduBuffer, &pduSize_n, 2);
    memcpy(pduBuffer + 2, dataBuffer, lengthOfData);

    // Send PDU
    int bytesSent = safeSend(clientSocket, pduBuffer, pduSize, 0);
    
    return bytesSent - 2;
}

int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize) {
    uint16_t pduSize, pduSize_n;
    
    // Get PDU Size
    int bytesRecv = safeRecv(socketNumber, (uint8_t *)(&pduSize_n), 2, MSG_WAITALL);
    if (bytesRecv == 0) {
        return bytesRecv;
    }

    pduSize = ntohs(pduSize_n);

    // Check buffer size
    if (pduSize - 2 > bufferSize) {
        fprintf(stderr, "Buffer is too small for receive. PDU Size: %d\tBuffer Size: %d\n", pduSize, bufferSize);
        exit(-1);
    }

    // Get PDU Data
    bytesRecv = safeRecv(socketNumber, dataBuffer, pduSize - 2, MSG_WAITALL);

    return bytesRecv;
}

