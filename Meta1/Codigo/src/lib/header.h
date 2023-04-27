#ifndef HEADER_H
#define HEADER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct GlobalData {
    int processIDs[10];
    int alertsWatcherID;

} Global;

typedef struct config_struct {
    int queue_size;
    int num_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

typedef struct shared_memory_struct {
    int *dados;
    int shmid;
} SharedMemory;

extern Global global;

extern Config config;

extern SharedMemory sharedMemory;

extern int debug;

#define MAX_STRING_SIZE 32
#define MIN_STRING_SIZE 3

int readConfigFile(char *filename);

int createSharedMemory();

int terminateAll();

void WorkerProcess();

void AlertsWatcherProcess();

void escreverLog(char *mensagem);

void *sensorReader();

void *consoleReader();

void *dispatcher();

#endif
