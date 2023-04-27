#include "lib/header.h"

// ================== Process Variables ==========================

// Processes IDs
int processIDs[10];
int alertsWatcherID;

// Threads
pthread_t sensorReaderThread, consoleReaderThread, dispatcherThread;

int debug = 1;

int parentPID;

SharedMemory sharedMemory;

Queue *queue;

// Global global;

// ================== Functions ==========================

// Function to add a new node to the queue
void addNodeToQueue(Queue *newNode) {
    Queue *currentNode = queue;

    // If the queue is empty
    if (currentNode == NULL) {
        queue = newNode;
        return;
    }

    // If the queue is not empty
    while (currentNode->next != NULL) {
        currentNode = currentNode->next;
    }

    currentNode->next = newNode;
}

// Function to pop the first node of the queue
Queue *popNodeFromQueue() {
    Queue *currentNode = queue;

    // If the queue is empty
    if (currentNode == NULL) {
        return NULL;
    }

    // If the queue is not empty
    queue = currentNode->next;

    return currentNode;
}

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

void *sensorReader() {
    escreverLog("Sensor Reader Thread Started\n");

    int fd_sensor_pipe;

    // Open the pipe
    if ((fd_sensor_pipe = open("sensorPipe", O_RDONLY)) < 0) {
        escreverLog("Error opening sensor_pipe\n");
        perror("Error opening sensor_pipe");
    }

    while (1) {
        // Read the message in SensorPipe
        char message[64];
        long size;

        size = read(fd_sensor_pipe, message, sizeof(message));

        if (size > MIN_STRING_SIZE) {
            printf("Message: %s\n", message);

            // Separate the message into tokens (sensorID, key, value), separated by #. Example: string#chave#87
            char *token;
            char *sensorID;
            char *key;
            int value;

            token = strtok(message, "#");
            sensorID = token;

            token = strtok(NULL, "#");
            key = token;

            token = strtok(NULL, "#");
            value = atoi(token);

            // Add the message to the queue
            Queue *newNode = (Queue *)malloc(sizeof(Queue));

            newNode->next = NULL;

            strcpy(newNode->ID, sensorID);
            strcpy(newNode->chave, key);
            newNode->valor = value;
            newNode->prioridade = 1;

            addNodeToQueue(newNode);

            if (debug) {
                printf("Queue:\n");

                // Print the queue
                Queue *currentNode = queue;

                while (currentNode != NULL) {
                    printf("ID: %s\n", currentNode->ID);
                    printf("Chave: %s\n", currentNode->chave);
                    printf("Valor: %d\n", currentNode->valor);
                    printf("Prioridade: %d\n", currentNode->prioridade);
                    currentNode = currentNode->next;
                }
            }
        }

        bzero(&message, sizeof(message));
    }

    pthread_exit(NULL);
}

void *consoleReader() {
    escreverLog("Console Reader Thread Started\n");

    int fd_console_pipe;

    // Open the pipe
    if ((fd_console_pipe = open("consolePipe", O_RDONLY)) < 0) {
        escreverLog("Error opening console_pipe\n");
        perror("Error opening console_pipe");
    }

    while (1) {
        // Read the command in ConsolePipe
        char command[64];
        long size;

        size = read(fd_console_pipe, command, sizeof(command));

        if (size > 1) {
            // printf("Command: %s\n", command);

            // Add the command to the queue
            Queue *newNode = (Queue *)malloc(sizeof(Queue));

            newNode->next = NULL;
            strcpy(newNode->command, command);
            newNode->prioridade = 9;

            addNodeToQueue(newNode);

            // if (debug) {
            //     printf("Queue:\n");

            //     // Print the queue
            //     Queue *currentNode = queue;

            //     while (currentNode != NULL) {
            //         printf("Command: %s\n", currentNode->command);
            //         currentNode = currentNode->next;
            //     }
            // }
        }

        bzero(command, sizeof(command));
    }

    pthread_exit(NULL);
}

void *dispatcher() {
    escreverLog("Dispatcher Thread Started\n");

    while (1) {
        // If the queue is not empty
        if (queue != NULL) {
            // Get the first node of the queue
            Queue *currentNode = popNodeFromQueue();

            if (currentNode != NULL) {
                // If the node is a command
                if (currentNode->prioridade == 9) {
                    if (debug) {
                        printf("Dispatcher:\n");
                        printf("Command: %s\n", currentNode->command);
                    }
                }

                // If the command is "exit"
                if (strcmp(currentNode->command, "exit") == 0) {
                    // Terminate the program
                    terminateAll();
                }

                // print the queue after the pop
                if (debug) {
                    printf("Queue:\n");

                    // Print the queue
                    Queue *aux = queue;

                    while (aux != NULL) {
                        printf("ID: %s\n", aux->ID);
                        printf("Chave: %s\n", aux->chave);
                        printf("Valor: %d\n", aux->valor);
                        printf("Prioridade: %d\n", aux->prioridade);
                        aux = aux->next;
                    }
                }

                // If the node is a message
                if (currentNode->prioridade == 1) {
                    if (debug) {
                        printf("Dispatcher:\n");
                        printf("ID: %s\n", currentNode->ID);
                    }

                    // If the message is from a sensor
                    if (strcmp(currentNode->chave, "temperatura") == 0 || strcmp(currentNode->chave, "humidade") == 0) {
                        // Send the message to the Alerts Watcher process
                        kill(alertsWatcherID, SIGUSR1);
                    }
                }
            }
        }

        sleep(1);
    }

    pthread_exit(NULL);
}

void terminateAll() {
    if (parentPID == getpid()) {
        // Terminate the shared memory
        shmdt(sharedMemory.dados);
        shmctl(sharedMemory.shmid, IPC_RMID, 0);

        if (debug)
            escreverLog("Shared memory terminated\n");

        // Terminate the threads
        pthread_cancel(sensorReaderThread);
        pthread_cancel(consoleReaderThread);
        pthread_cancel(dispatcherThread);

        if (debug)
            escreverLog("Threads terminated\n");

        // Wait for the worker processes to terminate
        for (int i = 1; i < config.num_workers + 1; i++) {
            waitpid(processIDs[i], NULL, 0);
        }

        // Wait for the Alerts Watcher process to terminate
        waitpid(alertsWatcherID, NULL, 0);

        if (debug)
            escreverLog("Processes terminated\n");

        escreverLog("Program terminated\n");

        // return 0;
        exit(0);
    } else {
        // Terminate the processes
        // escreverLog("Worker Process terminated\n");
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    // Receive the configuration file name as a parameter
    if (argc != 2) {
        printf("Error: Invalid number of parameters\n");
        return 0;
    }
    if (debug)
        escreverLog("System Manager Started\n");

    parentPID = getpid();

    // ==================================== Install CTRL+C signal =============================================
    signal(SIGINT, terminateAll);

    // ==================================== Read the configuration file ======================================

    if (readConfigFile(argv[1])) {
        escreverLog("ERROR READING CONFIG FILE\n");
        return 0;
    }

    // ================================ Create and attach the shared memory ==================================

    if (createSharedMemory()) {
        escreverLog("ERROR CREATING SHARED MEMORY\n");
        terminateAll();
    }

    // ===================================== Create the Worker Processes ======================================

    for (int i = 1; i < config.num_workers + 1; i++) {
        processIDs[i] = fork();

        if (processIDs[i] > 0) {
            // Wait for the child process to terminate

            continue;

        } else if (processIDs[i] == 0) {
            // Child process
            WorkerProcess(i);
            exit(0);

        } else {
            escreverLog("Error creating Worker Process\n");
            terminateAll();
        }
    }

    // ====================================== Create Alerts Watcher Process ==================================

    alertsWatcherID = fork();

    if (alertsWatcherID > 0) {
        // Continue the program

    } else if (alertsWatcherID == 0) {
        // Child process
        AlertsWatcherProcess(1);
        exit(0);

    } else {
        escreverLog("Error creating Alerts Watcher Process\n");
        perror("Error creating Alerts Watcher Process");
        terminateAll();
    }

    // ===================================== Create the Console Pipe and the Sensor Pipe =====================
    unlink("consolePipe");
    unlink("sensorPipe");

    if ((mkfifo("consolePipe", O_CREAT | O_EXCL | 0777) < 0 && errno != EEXIST)) {
        escreverLog("Error creating Console Pipe\n");
        perror("Error creating Console Pipe");
        terminateAll();
    }

    if ((mkfifo("sensorPipe", O_CREAT | O_EXCL | 0777) < 0 && errno != EEXIST)) {
        escreverLog("Error creating Sensor Pipe\n");
        perror("Error creating Sensor Pipe");
        terminateAll();
    }

    printf("Ola");

    // ====================================== Create the threads =============================================

    // Create Sensor Reader, Console Reader and Dispatcher threads
    if (pthread_create(&sensorReaderThread, NULL, sensorReader, NULL) != 0) {
        escreverLog("Error creating Sensor Reader Thread\n");
        perror("Error creating Sensor Reader Thread");
        terminateAll();
    }

    if (pthread_create(&consoleReaderThread, NULL, consoleReader, NULL) != 0) {
        escreverLog("Error creating Console Reader Thread\n");
        perror("Error creating Console Reader Thread");
        terminateAll();
    }

    if (pthread_create(&dispatcherThread, NULL, dispatcher, NULL) != 0) {
        escreverLog("Error creating Dispatcher Thread\n");
        perror("Error creating Dispatcher Thread");
        terminateAll();
    }

    // ===================================== Create the Message Queue ========================================

    // ===================================== Keep the program running ========================================

    while (1) {
        sleep(1);
    }

    return 0;
}