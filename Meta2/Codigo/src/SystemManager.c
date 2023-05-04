#include "header.h"

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

FILE *logFile;

sem_t *semaforo, *semaforo_log, *semaforo_alerts;

int shmid;

int msgqid;

// ================== Processes ==========================
void WorkerProcess(int id, int fd) {
    // Create String with the process ID
    char buf[BUFF_SIZE * 10];
    sprintf(buf, "Worker process ID %d created\n", id);

    sem_wait(semaforo_log);
    escreverLog(buf);
    sem_post(semaforo_log);

    while (1) {
        // Read from the pipe
        char buffer[BUFF_SIZE * 4];
        char *token;
        Message msg;

        int r = read(fd, buffer, sizeof(buffer));

        if (r == -1) {
            perror("read");
            exit(1);
        }

        if (r > 1) {
            // Print the message
            // sprintf(buf, "Worker %d received: %s\n", id, buffer);
            // escreverLog(buf);

            char aux[BUFF_SIZE * 4];
            strcpy(aux, buffer);

            // Parse the message to check if it's from a sensor or a console
            token = strtok(aux, ";");

            // printf("Token: %s\n", token);

            if (strcmp(token, "S") == 0) {
                // Parse the message ID#key#value
                token = strtok(NULL, ";");

                char *sensorID = strtok(token, "#");
                char *key = strtok(NULL, "#");
                char *value = strtok(NULL, "#");

                // printf("Key: %s\n", key);

                sem_wait(semaforo);

                // Add to the key queue
                for (int i = 0; i < KEY_SIZE; i++) {
                    if (strcmp(sharedMemory->key_queue[i].chave, key) == 0) {
                        // printf("AQUI2\n");
                        sharedMemory->key_queue[i].last = atoi(value);
                        sharedMemory->key_queue[i].count++;
                        sharedMemory->key_queue[i].media = (sharedMemory->key_queue[i].media + atoi(value)) / sharedMemory->key_queue[i].count;

                        if (atoi(value) < sharedMemory->key_queue[i].min) {
                            sharedMemory->key_queue[i].min = atoi(value);
                        }

                        if (atoi(value) > sharedMemory->key_queue[i].max) {
                            sharedMemory->key_queue[i].max = atoi(value);
                        }

                        break;
                    } else if (strcmp(sharedMemory->key_queue[i].chave, "") == 0) {
                        // printf("AQUI3\n");
                        strcpy(sharedMemory->key_queue[i].chave, key);
                        strcpy(sharedMemory->key_queue[i].ID, sensorID);
                        sharedMemory->key_queue[i].last = atoi(value);
                        sharedMemory->key_queue[i].count = 1;
                        sharedMemory->key_queue[i].media = atoi(value);
                        sharedMemory->key_queue[i].min = atoi(value);
                        sharedMemory->key_queue[i].max = atoi(value);

                        break;
                    }
                }

                sem_post(semaforo);

            } else if (strncmp(token, "C", 1) == 0) {
                // Parse the command
                token = strtok(NULL, ";");

                // printf("Token: %s\n", token);

                char *consoleID = strtok(NULL, ";");

                // printf("Console ID: %s\n", consoleID);

                msg.type = atoi(consoleID);

                // If the command is "stats", print the key queue
                if (strcmp(token, "stats") == 0) {
                    sem_wait(semaforo);

                    // Print Key Queue with format -> key last min max media count
                    for (int i = 0; i < KEY_SIZE; i++) {
                        if (strcmp(sharedMemory->key_queue[i].chave, "") != 0) {
                            sprintf(msg.content, "%s %d %d %d %.2f %d", sharedMemory->key_queue[i].chave, sharedMemory->key_queue[i].last, sharedMemory->key_queue[i].min, sharedMemory->key_queue[i].max, sharedMemory->key_queue[i].media, sharedMemory->key_queue[i].count);

                            if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                                perror("msgsnd");
                                printf("Message: %s\n", msg.content);
                                printf("Message size: %ld\n", sizeof(msg) - sizeof(long));
                                printf("Message type: %ld\n", msg.type);
                                printf("Msqid: %d\n", msgqid);
                                exit(1);
                            }
                        }
                    }

                    sem_post(semaforo);
                } else if (strcmp(token, "reset") == 0) {
                    // If the key queue is empy, send "ERROR" to the console
                    int empty = 1;

                    sem_wait(semaforo);

                    for (int i = 0; i < KEY_SIZE; i++) {
                        if (strcmp(sharedMemory->key_queue[i].chave, "") != 0) {
                            empty = 0;
                            break;
                        }
                    }

                    sem_post(semaforo);

                    if (empty) {
                        strcpy(msg.content, "ERROR");

                        if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(1);
                        }

                        // printf("Message sent to console\n");

                        continue;
                    }
                    // Clear the key queue
                    sem_wait(semaforo);

                    for (int i = 0; i < KEY_SIZE; i++) {
                        sharedMemory->key_queue[i].last = 0;
                        sharedMemory->key_queue[i].count = 0;
                        sharedMemory->key_queue[i].media = 0;
                        sharedMemory->key_queue[i].min = 0;
                        sharedMemory->key_queue[i].max = 0;
                    }

                    sem_post(semaforo);

                    // Send the message to the console

                    strcpy(msg.content, "OK");

                    if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                        perror("msgsnd");
                        exit(1);
                    }

                } else if (strcmp(token, "sensors") == 0) {
                    // Print the key queus chave
                    sem_wait(semaforo);

                    for (int i = 0; i < KEY_SIZE; i++) {
                        if (strcmp(sharedMemory->key_queue[i].chave, "") != 0) {
                            sprintf(msg.content, "%s", sharedMemory->key_queue[i].chave);

                            if (msgsnd(msgqid, &msg, sizeof(msg), 0) == -1) {
                                perror("msgsnd");
                                exit(1);
                            }

                            // printf("Message sent to console\n");
                        }
                    }

                    sem_post(semaforo);
                } else if (strncmp(token, "add_alert", strlen("add_alert")) == 0) {
                    // Separar o token em argumentos
                    token = strtok(token, " ");
                    char *args[5];
                    int i = 0;

                    while (token != NULL) {
                        args[i] = token;
                        token = strtok(NULL, " ");
                        i++;
                    }

                    // Adicionar à alert queue
                    sem_wait(semaforo);

                    for (i = 0; i < ALERTS_SIZE; i++) {
                        if (strcmp(sharedMemory->alert_queue[i].id, "") == 0) {
                            strcpy(sharedMemory->alert_queue[i].id, args[1]);
                            strcpy(sharedMemory->alert_queue[i].chave, args[2]);
                            sharedMemory->alert_queue[i].min = atoi(args[3]);
                            sharedMemory->alert_queue[i].max = atoi(args[4]);
                            sharedMemory->alert_queue[i].consoleID = atoi(consoleID);
                            break;
                        }
                    }

                    sem_post(semaforo);

                    Message msg;
                    msg.type = atoi(consoleID);

                    // Send the message to the console
                    strcpy(msg.content, "OK");

                    if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                        perror("msgsnd");
                        exit(1);
                    }

                    // printf("Message sent to console: %s\n", msg.content);

                } else if (strcmp(token, "list_alerts") == 0) {
                    sem_wait(semaforo);
                    for (int i = 0; i < ALERTS_SIZE; i++) {
                        Message msg;
                        msg.type = atoi(consoleID);
                        if (strcmp(sharedMemory->alert_queue[i].id, "") != 0) {
                            sprintf(msg.content, "%s %s %d %d", sharedMemory->alert_queue[i].id, sharedMemory->alert_queue[i].chave, sharedMemory->alert_queue[i].min, sharedMemory->alert_queue[i].max);
                            if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                                perror("msgsnd");
                                exit(1);
                            }
                        }
                    }
                    sem_post(semaforo);
                } else if (strncmp(token, "remove_alert", strlen("remove-alert")) == 0) {
                    // Remove the alert from the alert queue
                    token = strtok(token, " ");
                    char *args[2];
                    int i = 0;

                    while (token != NULL) {
                        args[i] = token;
                        token = strtok(NULL, " ");
                        i++;
                    }

                    sem_wait(semaforo);

                    for (i = 0; i < ALERTS_SIZE; i++) {
                        if (strcmp(sharedMemory->alert_queue[i].id, args[1]) == 0) {
                            strcpy(sharedMemory->alert_queue[i].id, "");
                            strcpy(sharedMemory->alert_queue[i].chave, "");
                            sharedMemory->alert_queue[i].min = 0;
                            sharedMemory->alert_queue[i].max = 0;
                            sharedMemory->alert_queue[i].consoleID = 0;
                            break;
                        }
                    }

                    sem_post(semaforo);

                    Message msg;
                    msg.type = atoi(consoleID);

                    // Send the message to the console
                    strcpy(msg.content, "OK");

                    if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                        perror("msgsnd");
                        exit(1);
                    }
                }
            }

            // Clear the buffer and the token
        }
        memset(buffer, 0, sizeof(buffer));
        token = NULL;
        sem_post(semaforo);
    }
}

void AlertsWatcherProcess(int id) {
    char buf[64];
    sprintf(buf, "Alerts watcher process ID %d created\n", id);

    int printed;

    sem_wait(semaforo_log);
    escreverLog(buf);
    sem_post(semaforo_log);

    while (1) {
        for (int i = 0; i < ALERTS_SIZE; i++) {
            if (strcmp(sharedMemory->alert_queue[i].id, "") != 0) {
                for (int j = 0; j < KEY_SIZE; j++) {
                    if (strcmp(sharedMemory->key_queue[j].chave, sharedMemory->alert_queue[i].chave) == 0) {
                        if (printed == sharedMemory->key_queue[j].last) {
                            continue;
                        }

                        if (sharedMemory->key_queue[j].last < sharedMemory->alert_queue[i].min || sharedMemory->key_queue[j].last > sharedMemory->alert_queue[i].max) {
                            Message msg;
                            msg.type = sharedMemory->alert_queue[i].consoleID;
                            sprintf(msg.content, "ALERT %s (%s %d to %d) TRIGGERED", sharedMemory->alert_queue[i].id, sharedMemory->key_queue[j].chave, sharedMemory->alert_queue[i].min, sharedMemory->alert_queue[i].max);

                            if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                                perror("msgsnd");
                                exit(1);
                            }

                            sem_wait(semaforo_log);
                            escreverLog(msg.content);
                            sem_post(semaforo_log);

                            printed = sharedMemory->key_queue[j].last;
                        }
                    }
                }
            }
        }
    }
}

// ================== Functions ==========================

void escreverLog(char *mensagem) {
    time_t my_time;
    struct tm *timeInfo;
    time(&my_time);
    timeInfo = localtime(&my_time);

    // Write the message to the log file with the date before with the format: "HH:MM:SS MESSAGE"
    if (fprintf(logFile, "%d:%d:%d -> %s\n", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec, mensagem) < 0) {
        perror("Error writing to log file");
        exit(1);
    } else {
        fflush(logFile);
    }

    // Print the message to the console
    printf("%d:%d:%d -> %s\n", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec, mensagem);
}

void terminateAll() {
    if (parentPID == getpid()) {
        sem_wait(semaforo_log);
        escreverLog("Terminating Program\n");
        sem_post(semaforo_log);

        // Terminate the shared memory
        shmdt(sharedMemory);
        shmctl(shmid, IPC_RMID, 0);

        // Terminate the threads
        pthread_cancel(sensorReaderThread);
        pthread_cancel(consoleReaderThread);
        pthread_cancel(dispatcherThread);

        // Wait for the worker processes to terminate
        for (int i = 1; i < config.num_workers + 1; i++) {
            waitpid(processIDs[i], NULL, 0);
        }

        // Wait for the Alerts Watcher process to terminate
        waitpid(alertsWatcherID, NULL, 0);

        // Close the message queue
        msgctl(msgqid, IPC_RMID, NULL);

        // Close the semaphore
        sem_close(semaforo);
        sem_unlink(SEM_NAME);
        sem_close(semaforo_log);
        sem_unlink(SEM_LOG_NAME);
        sem_close(semaforo_alerts);
        sem_unlink(SEM_ALERTS_NAME);

        fclose(logFile);

        exit(0);
    } else {
        // Terminate the child process
        exit(0);
    }
}

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
        perror("shmget() error\n");
        return 1;
    }

    // Attach the shared memory
    if ((sharedMemory = shmat(shmid, NULL, 0)) < 0) {
        perror("shmat() error\n");
        return 1;
    }

    sem_wait(semaforo_log);
    escreverLog("Shared memory created\n");
    sem_post(semaforo_log);

    return 0;
}

void *sensorReader() {
    sem_wait(semaforo_log);
    escreverLog("Sensor Reader Thread Started\n");
    sem_post(semaforo_log);

    int fd_sensor_pipe;

    // Open the pipe
    if ((fd_sensor_pipe = open("sensorPipe", O_RDONLY)) < 0) {
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
    sem_wait(semaforo_log);
    escreverLog("Console Reader Thread Started\n");
    sem_post(semaforo_log);

    int fd_console_pipe;

    // Open the pipe
    if ((fd_console_pipe = open("consolePipe", O_RDONLY)) < 0) {
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
    sem_wait(semaforo_log);
    escreverLog("Dispatcher Thread Started\n");
    sem_post(semaforo_log);

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

    parentPID = getpid();

    // ==================================== Open Log File ====================================================
    if ((logFile = fopen("log.txt", "a")) == NULL) {
        perror("Error opening file\n");
        exit(0);
    }

    // ==================================== Install CTRL+C signal =============================================
    signal(SIGINT, terminateAll);

    // ===================================== Create the Semaphores ==========================================
    sem_unlink(SEM_NAME);
    if ((semaforo = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) {
        perror("Error creating semaphore");
        terminateAll();
    }

    sem_unlink(SEM_LOG_NAME);
    if ((semaforo_log = sem_open(SEM_LOG_NAME, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) {
        perror("Error creating semaphore");
        terminateAll();
    }

    sem_unlink(SEM_ALERTS_NAME);
    if ((semaforo_alerts = sem_open(SEM_ALERTS_NAME, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) {
        perror("Error creating semaphore");
        terminateAll();
    }

    // ==================================== Read the configuration file ======================================

    if (readConfigFile(argv[1])) {
        perror("ERROR READING CONFIG FILE\n");
        return 0;
    }

    // ================================ Create and attach the shared memory ==================================

    if (createSharedMemory()) {
        perror("ERROR CREATING SHARED MEMORY\n");
        terminateAll();
    }

    // ===================================== Create the Message Queue ========================================
    key_t key = ftok(".", MSQ_KEY);
    if ((msgqid = msgget(key, IPC_CREAT | 0777)) < 0) {
        perror("Error creating message queue");
        terminateAll();
    }

    // ===================================== Create the Pipes =====================
    unlink("consolePipe");
    unlink("sensorPipe");

    // Console Pipe
    if ((mkfifo("consolePipe", O_CREAT | O_EXCL | 0777) < 0 && errno != EEXIST)) {
        perror("Error creating Console Pipe");
        terminateAll();
    }

    // Sensor Pipe
    if ((mkfifo("sensorPipe", O_CREAT | O_EXCL | 0777) < 0 && errno != EEXIST)) {
        perror("Error creating Sensor Pipe");
        terminateAll();
    }

    // Unnamed Pipes
    int fd_unnamed_pipe[config.num_workers][2];

    for (int i = 0; i < config.num_workers; i++) {
        if (pipe(fd_unnamed_pipe[i]) == -1) {
            perror("Error creating Unnamed Pipe");
            terminateAll();
        }
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
            perror("Error creating Worker Process\n");
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
        perror("Error creating Alerts Watcher Process");
        terminateAll();
    }

    // ====================================== Create the threads =============================================

    // Create Sensor Reader, Console Reader and Dispatcher threads
    if (pthread_create(&sensorReaderThread, NULL, sensorReader, NULL) != 0) {
        perror("Error creating Sensor Reader Thread");
        terminateAll();
    }

    if (pthread_create(&consoleReaderThread, NULL, consoleReader, NULL) != 0) {
        perror("Error creating Console Reader Thread");
        terminateAll();
    }

    if (pthread_create(&dispatcherThread, NULL, dispatcher, fd_unnamed_pipe) != 0) {
        perror("Error creating Dispatcher Thread");
        terminateAll();
    }

    // ===================================== Keep the program running ========================================

    while (1) {
    }

    return 0;
}