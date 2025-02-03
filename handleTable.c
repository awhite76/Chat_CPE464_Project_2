#include "handleTable.h"
#include "safeUtil.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Globals
struct TableEntry *table;
int tableCapacity;
int tablePopulation;

void initTable() {
    // Allocate 5 entries to the table
    table = (struct TableEntry *)sCalloc(5, sizeof(struct TableEntry));
    tableCapacity = 5;
    tablePopulation = 0;
}

int getTablePopulation() {
    return tablePopulation;
}

int lookUpHandle(char *handle) {
    // Iterate over table entries
    for (int i = 0; i < tableCapacity; i++) {        
        // Compare current handle with searching handle
        if (strcmp(handle, table[i].handle) == 0) {
            return table[i].socket;
        }
    }

    // Could not find handle in table
    return -1;
}

char *lookUpBysocket(int socket) {
    // Iterate over table entries
    for (int i = 0; i < tableCapacity; i++) {        
        // Compare current socket with searching socket
        if (socket == table[i].socket) {
            return table[i].handle;
        }
    }

    // Could not find socket in table
    return NULL;
}

int addHandle(char *handle, int socket) {
    // Check if you need to increase table size
    if (tablePopulation + 1 >= tableCapacity) {
        tableCapacity += 5;
        table = srealloc(table, tableCapacity * sizeof(struct TableEntry));
    }

    // Iterate over table, store first open index, and check for matching handle
    int idx = -1;
    for (int i = 0; i < tableCapacity; i++) {        
        // Compare current handle with empty string
        if (idx == -1 && strcmp(table[i].handle, "") == 0) {
            idx = i;
        }

        // Compare current handle with searching handle
        if (strcmp(table[i].handle, handle) == 0) {
            fprintf(stderr, "Handle already in handle table\n");
            return -1;
        }
    }

    // Insert handle and socket into table
    strcpy(table[idx].handle, handle);
    table[idx].socket = socket;
    tablePopulation += 1;
    return 0;
}

void removeHandle(char *handle) {
    // Iterate over table until you find table entry with given handle
    for (int i = 0; i < tableCapacity; i++) {        
        // Compare current socket with searching socket
        if (strcmp(table[i].handle, handle) == 0) {
            table[i].handle[0] = '\0';
            table[i].socket = 0;
            tablePopulation -= 1;
            return;
        }
    }
}

uint8_t getIthHandle(int idx, char *handle) { // 1 based indexing
    // Iterate over table until you get the ith handle
    int count = 0;
    uint8_t handleLen = 0;
    for (int i = 0; i < tableCapacity; i++) {
        // If table entry is not empty increment the count
        if (strcmp(table[i].handle, "")) {
            count++;
        }

        // Copy handle to buffer and return length
        if (count == idx) {
            handleLen = strlen(table[i].handle);
            memcpy(handle, table[i].handle, handleLen);
            return handleLen;
        }
    }

    return handleLen;
}


