#ifndef SENDRECVUTIL_H
#define SENDRECVUTIL_H

#include <stdint.h>

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData, uint8_t *flag);
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize, uint8_t *flag);
void safeSendPDU(int socketNum, uint8_t *data_buff, int sendLen, uint8_t *flag);

#endif