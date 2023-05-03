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

// typedef struct GlobalData {
//     int fd_console_pipe, fd_sensor_pipe;

// } Global;

#define MAX_STRING_SIZE 32
#define MIN_STRING_SIZE 3
#define BUFF_SIZE 2048
#define MSQ_KEY 0x1234
#define SEM_NAME "Semaforo"

typedef struct config_struct {
    int queue_size;
    int num_workers;
    int max_keys;
    int max_sensors;
    int max_alerts;
} Config;

typedef struct message {
    long type;
    char content[BUFF_SIZE];

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

typedef struct shared_memory_struct {
    KeyQueue *key_queue;
} SharedMemory;

extern Config config;

extern SharedMemory *sharedMemory;

extern Queue *queue;

extern int debug;

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
