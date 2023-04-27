#include "lib/header.h"

SharedMemory sharedMemory;

int createSharedMemory() {
    // Create the shared memory
    if ((sharedMemory.shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) == -1) {
        escreverLog("shmget() error\n");
        return 1;
    }

    // Attach the shared memory
    if ((sharedMemory.dados = shmat(sharedMemory.shmid, NULL, 0)) < 0) {
        escreverLog("shmat() error\n");
        return 1;
    }

    escreverLog("Shared memory created\n");

    return 0;
}

int terminateAll(pthread_t sensorReaderThread, pthread_t consoleReaderThread, pthread_t dispatcherThread) {
    // Give some time for everything to start
    sleep(1);

    // Terminate the shared memory
    shmdt(sharedMemory.dados);
    shmctl(sharedMemory.shmid, IPC_RMID, 0);

    if (debug)
        printf("Shared memory terminated\n");

    // Wait for the threads to terminate
    pthread_join(sensorReaderThread, NULL);
    pthread_join(consoleReaderThread, NULL);
    pthread_join(dispatcherThread, NULL);

    // * Tentativa de terminar os processos e threads

    // TODO: Corrigir isto e verificar se e assim que se faz -> Meta 2

    // // Terminate the threads
    // pthread_cancel(sensorReaderThread);
    // pthread_cancel(consoleReaderThread);
    // pthread_cancel(dispatcherThread);

    // if (debug)
    //     printf("\nThreads terminated\n\n");

    // // Terminate the processes except the parent process
    // for (int i = 1; i < config.num_workers + 1; i++) {
    //     kill(global.processIDs[i], SIGKILL);

    //     if (debug)
    //         printf("Worker process %d terminated\n", global.processIDs[i]);
    // }

    // kill(global.alertsWatcherID, SIGKILL);
    // if (debug)
    //     printf("\nAlerts Watcher process %d terminated\n\n", global.alertsWatcherID);

    escreverLog("Program terminated\n");

    return 0;
}

void *sensorReader() {
    escreverLog("Sensor Reader Thread Started\n");
    pthread_exit(NULL);
}

void *consoleReader() {
    escreverLog("Console Reader Thread Started\n");
    pthread_exit(NULL);
}

void *dispatcher() {
    escreverLog("Dispatcher Thread Started\n");
    pthread_exit(NULL);
}