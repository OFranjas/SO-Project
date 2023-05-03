#include "lib/header.h"

// ================== Variables ==========================

// Processes IDs
int processIDs[10];
int alertsWatcherID;

// Threads
pthread_t sensorReaderThread, consoleReaderThread, dispatcherThread;

int debug = 0;

int parentPID;

SharedMemory *sharedMemory;

Queue *queue;

KeyQueue *keyQueue;

sem_t *semaforo;

// ================== Processes ==========================
void WorkerProcess(int id, int fd) {
    // Create String with the process ID
    char buf[BUFF_SIZE * 10];
    sprintf(buf, "Worker process ID %d created\n", id);

    escreverLog(buf);

    while (1) {
        // Read from the pipe
        char buffer[BUFF_SIZE * 4];
        char *token;

        int r = read(fd, buffer, sizeof(buffer));

        if (r == -1) {
            perror("read");
            exit(1);
        }

        if (r > 1) {
            // Print the message
            sprintf(buf, "Worker %d received: %s\n", id, buffer);
            escreverLog(buf);

            char aux[BUFF_SIZE * 4];
            strcpy(aux, buffer);

            // Parse the message to check if it's from a sensor or a console
            token = strtok(aux, ";");

            printf("Token: %s\n", token);

            // sem_wait(semaforo);

            if (strcmp(token, "S") == 0) {
                // Parse the message ID#key#value
                token = strtok(NULL, ";");

                char *sensorID = strtok(token, "#");
                char *key = strtok(NULL, "#");
                char *value = strtok(NULL, "#");

                printf("Key: %s\n", key);

                // Add to the key queue
                KeyQueue *currentKey = sharedMemory.key_queue;

                // Verify if the key already exists
                while (currentKey != NULL) {
                    if (strcmp(currentKey->chave, key) == 0) {
                        // If it exists, update the values
                        currentKey->last = atoi(value);
                        currentKey->count++;
                        currentKey->media = (currentKey->media + atoi(value)) / currentKey->count;

                        if (atoi(value) < currentKey->min) {
                            currentKey->min = atoi(value);
                        }

                        if (atoi(value) > currentKey->max) {
                            currentKey->max = atoi(value);
                        }

                        break;
                    }

                    currentKey = currentKey->next;
                }

                // If it doesn't exist, create a new one
                if (currentKey == NULL) {
                    KeyQueue *newKey = malloc(sizeof(KeyQueue));

                    strcpy(newKey->chave, key);
                    strcpy(newKey->ID, sensorID);
                    newKey->last = atoi(value);
                    newKey->count = 1;
                    newKey->media = atoi(value);
                    newKey->min = atoi(value);
                    newKey->max = atoi(value);

                    newKey->next = sharedMemory.key_queue;
                    sharedMemory.key_queue = newKey;
                }

            } else if (strncmp(token, "C", 1) == 0) {
                // Parse the command
                token = strtok(NULL, ";");

                printf("Command: %s\n", token);

                // If the command is "stats", print the key queue
                if (strcmp(token, "stats") == 0) {
                    KeyQueue *currentKey = sharedMemory.key_queue;

                    while (currentKey != NULL) {
                        printf("AQUI3\n");
                        printf("Key: %s\n", currentKey->chave);
                        printf("ID: %s\n", currentKey->ID);
                        printf("Last: %d\n", currentKey->last);
                        printf("Count: %d\n", currentKey->count);
                        printf("Media: %f\n", currentKey->media);
                        printf("Min: %d\n", currentKey->min);
                        printf("Max: %d\n", currentKey->max);

                        currentKey = currentKey->next;
                    }
                }
            }

            // sem_post(semaforo);

            // Clear the buffer and the token
        }
        memset(buffer, 0, sizeof(buffer));
        token = NULL;
    }
}

    escreverLog(buf);
}
// ================== Functions ==========================

// Function to add a new node to the queue
void addNodeToQueue(Queue *newNode) {
    Queue *currentNode = queue;

    // If the queue is empty, add the node to the queue
    if (currentNode == NULL) {
        queue = newNode;
        return;
    }

    // If the queue is not empty, add the node to the end of the queue
    while (currentNode->next != NULL) {
        currentNode = currentNode->next;
    }

    currentNode->next = newNode;
}

// Function to pop the first node of the queue
Queue *popNodeFromQueue() {
    Queue *currentNode = queue;

    // If the queue is empty, return NULL
    if (currentNode == NULL) {
        return NULL;
    }

    // If the queue is not empty, pop the first node of the queue
    queue = currentNode->next;

    return currentNode;
}

int createSharedMemory() {
    // Create the shared memory
    if ((shmid = shmget(IPC_PRIVATE, sizeof(sharedMemory) + sizeof(sharedMemory->key_queue) * config.max_keys, IPC_CREAT | 0777)) == -1) {
        escreverLog("shmget() error\n");
        return 1;
    }

    // Attach the shared memory
    if ((sharedMemory = shmat(shmid, NULL, 0)) < 0) {
        escreverLog("shmat() error\n");
        return 1;
    }

    sharedMemory.key_queue = NULL;

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
            if (debug)
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

void *dispatcher(void *arg) {
    escreverLog("Dispatcher Thread Started\n");

    // Get the argument fd_unnamed_pipe[config.num_workers][2]
    int(*fd_unnamed_pipe)[2] = (int(*)[2])arg;

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

                    // Write to a random worker unnamed pipe
                    int randomWorker = rand() % config.num_workers;

                    if (debug) {
                        printf("Random Worker: %d\n", randomWorker);
                    }

                    char message[BUFF_SIZE * 4];
                    sprintf(message, "C;%s", currentNode->command);

                    // Write to the worker
                    if (write(fd_unnamed_pipe[randomWorker][1], message, sizeof(currentNode->command)) < 0) {
                        escreverLog("Error writing to worker\n");
                        perror("Error writing to worker");
                    }
                }

                // If the node is a message
                if (currentNode->prioridade == 1) {
                    if (debug) {
                        printf("Dispatcher:\n");
                        printf("ID: %s\n", currentNode->ID);
                    }

                    // Turn the message into a string of format -> ID#chave#valor
                    char message[BUFF_SIZE * 4];
                    sprintf(message, "S;%s#%s#%d", currentNode->ID, currentNode->chave, currentNode->valor);

                    if (debug) {
                        printf("Message: %s\n", message);
                    }

                    // Write to a random worker unnamed pipe
                    int randomWorker = rand() % config.num_workers;

                    if (debug) {
                        printf("Random Worker: %d\n", randomWorker);
                    }

                    // Write to the worker
                    if (write(fd_unnamed_pipe[randomWorker][1], message, sizeof(message)) < 0) {
                        escreverLog("Error writing to worker\n");
                        perror("Error writing to worker");
                    }
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
            }
        }

        sleep(1);
    }

    pthread_exit(NULL);
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

    for (int i = 0; i < config.num_workers; i++) {
        processIDs[i] = fork();

        if (processIDs[i] > 0) {
            // Wait for the child process to terminate

            continue;

        } else if (processIDs[i] == 0) {
            // Child process
            WorkerProcess(i, fd_unnamed_pipe[i][0]);
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

    if (pthread_create(&dispatcherThread, NULL, dispatcher, fd_unnamed_pipe) != 0) {
        escreverLog("Error creating Dispatcher Thread\n");
        perror("Error creating Dispatcher Thread");
        terminateAll();
    }

    // ===================================== Keep the program running ========================================

    while (1) {
    }

    return 0;
}