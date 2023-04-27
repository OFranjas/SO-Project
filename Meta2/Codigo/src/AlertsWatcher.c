#include "lib/header.h"

void AlertsWatcherProcess(int id) {
    char buf[64];
    sprintf(buf, "Alerts watcher process ID %d created\n", id);

    escreverLog(buf);
}