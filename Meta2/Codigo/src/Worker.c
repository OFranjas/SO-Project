#include "lib/header.h"

void WorkerProcess(int id) {
    // Create String with the process ID
    char buf[64];
    sprintf(buf, "Worker process ID %d created\n", id);

    escreverLog(buf);
}