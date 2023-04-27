#include "lib/header.h"

int debug = 1;

Global global;

int main(int argc, char *argv[]) {
    // Receive the configuration file name as a parameter
    if (argc != 2) {
        printf("Error: Invalid number of parameters\n");
        return 1;
    }
    if (debug)
        escreverLog("System Manager Started\n");

    // ==================================== Read the configuration file ======================================

    if (readConfigFile(argv[1])) {
        escreverLog("ERROR READING CONFIG FILE\n");
        return 1;
    }

    // ================================ Create and attach the shared memory ==================================

    if (createSharedMemory()) {
        escreverLog("ERROR CREATING SHARED MEMORY\n");
        return 1;
    }

    // ===================================== Create the Worker Processes ======================================

    // int processIDs[config.num_workers];

    for (int i = 1; i < config.num_workers + 1; i++) {
        global.processIDs[i] = fork();

        if (global.processIDs[i] > 0) {
            // Wait for the child process to terminate

            // * Depois tirar isto, os processos devem manter-se a correr. Apenas fiz para testar

            wait(NULL);

        } else if (global.processIDs[i] == 0) {
            // Child process
            WorkerProcess(i);
            return 0;
        } else {
            escreverLog("Error creating Worker Process\n");
            return 1;
        }
    }

    // ====================================== Create Alerts Watcher Process ==================================

    global.alertsWatcherID = fork();

    if (global.alertsWatcherID > 0) {
        // Wait for the child process to terminate
        wait(NULL);

    } else if (global.alertsWatcherID == 0) {
        // Child process
        AlertsWatcherProcess(1);
        return 0;
    } else {
        escreverLog("Error creating Alerts Watcher Process\n");
        return 1;
    }

    // ====================================== Create the threads =============================================

    // Create Sensor Reader, Console Reader and Dispatcher threads
    pthread_t sensorReaderThread, consoleReaderThread, dispatcherThread;

    // Try to create the threads
    if (pthread_create(&sensorReaderThread, NULL, sensorReader, NULL) != 0) {
        escreverLog("Error creating Sensor Reader Thread\n");
        return 1;
    }

    if (pthread_create(&consoleReaderThread, NULL, consoleReader, NULL) != 0) {
        escreverLog("Error creating Console Reader Thread\n");
        return 1;
    }

    if (pthread_create(&dispatcherThread, NULL, dispatcher, NULL) != 0) {
        escreverLog("Error creating Dispatcher Thread\n");
        return 1;
    }

    // ===================================== Create the Message Queue ========================================

    // ===================================== Create Data Structure INTERNAL_QUEUE =============================

    // ===================================== Terminate the processes ========================================

    terminateAll(sensorReaderThread, consoleReaderThread, dispatcherThread);

    return 0;
}