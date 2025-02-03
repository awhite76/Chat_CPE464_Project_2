#ifndef HANDLE_TABLE_H
#define HANDLE_TABLE_H

#include <stdint.h>

struct TableEntry {
    char handle[101];
    int socket;
};

void initTable();
int lookUpHandle(char *handle);
char *lookUpBysocket(int socket);
int addHandle(char *handle, int socket);
void removeHandle(char *handle);
int getTablePopulation();
uint8_t getIthHandle(int idx, char *handle);

#endif