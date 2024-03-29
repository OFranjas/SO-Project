#ifndef HEADER_H
#define HEADER_H

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORKERS 999
#define MAX_STRING_SIZE 32
#define MIN_STRING_SIZE 3

#define BUFF_SIZE 2048
#define KEY_SIZE 4096
#define ALERTS_SIZE 4096
#define QUEUE_SIZE 4096

#define SENSORS_SIZE 4096
#define MSQ_KEY 0x4209
#define SEM_NAME "Semaforo"
#define SEM_LOG_NAME "SemaforoLog"
#define SEM_ALERTS_NAME "SemaforoAlerts"
#define SEM_KEY_NAME "SemaforoKey"

typedef struct config_struct {
    int queue_size;
    int num_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

typedef struct message {
    long type;
    char content[BUFF_SIZE * 3];

} Message;

// FIFO queue
typedef struct internal_queue {
    struct internal_queue *next;

    char ID[BUFF_SIZE];
    char chave[BUFF_SIZE];
    char command[BUFF_SIZE];
    int valor;
    int prioridade;
} Queue;

typedef struct key_queue {
    struct key_queue *next;

    char chave[BUFF_SIZE];
    char ID[BUFF_SIZE];
    int last;
    int min;
    int max;
    double media;
    int count;

} KeyQueue;

typedef struct alert_queue {
    char id[BUFF_SIZE];
    char chave[BUFF_SIZE];
    int min;
    int max;

    int consoleID;

} AlertQueue;

typedef struct sensor_queue {
    char id[BUFF_SIZE];

} SensorQueue;

typedef struct shared_memory_struct {
    KeyQueue key_queue[KEY_SIZE];
    AlertQueue alert_queue[KEY_SIZE];
    SensorQueue sensor_queue[KEY_SIZE];
    int ocupados[MAX_WORKERS];
    int queue_size;
    int alerts_size;
    int key_size;
} SharedMemory;

extern Config config;

extern SharedMemory *sharedMemory;

extern Queue *queue;

extern FILE *logFile;

extern int debug;

extern sem_t *semaforo_log;

int readConfigFile(char *filename);

int createSharedMemory();

void terminateAll();

void WorkerProcess();

void AlertsWatcherProcess();

void escreverLog(char *mensagem);

void *sensorReader();

void *consoleReader();

void *dispatcher();

#endif
