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

sem_t *semaforo, *semaforo_log, *semaforo_alerts, *semaforo_keys;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int shmid;

int msgqid;

// ================== Processes ==========================
void WorkerProcess(int id, int fd) {
    // Create String with the process ID
    char buf[BUFF_SIZE * 10];
    sprintf(buf, "WORKER %d CREATED\n", id);

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
            // Save in shared memory that worker is busy
            sem_wait(semaforo);
            sharedMemory->ocupados[id] = 1;
            sem_post(semaforo);

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

                // Verify if key queue is full
                sem_wait(semaforo);
                if (sharedMemory->key_size == config.max_keys) {
                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s %s %s QUEUE IS FULL\n", id, sensorID, key, value);
                    escreverLog(buffa);
                    sem_post(semaforo);

                    // Make the worker available again
                    sem_wait(semaforo);
                    sharedMemory->ocupados[id] = 0;
                    sem_post(semaforo);
                    continue;
                }

                sem_post(semaforo);

                sem_wait(semaforo);

                // Add to the key queue
                for (int i = 0; i < config.max_keys; i++) {
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
                sem_post(semaforo_keys);

                // Write to the log file
                sem_wait(semaforo_log);
                char buffa[4096 * 4];
                sprintf(buffa, "WORKER%d: %s %s %s PROCESSING COMPLETED\n", id, sensorID, key, value);
                escreverLog(buffa);
                sem_post(semaforo_log);

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
                    for (int i = 0; i < config.max_keys; i++) {
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

                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, token);
                    escreverLog(buffa);
                    sem_post(semaforo_log);

                } else if (strcmp(token, "reset") == 0) {
                    // If the key queue is empy, send "ERROR" to the console
                    int empty = 1;

                    sem_wait(semaforo);

                    for (int i = 0; i < config.max_keys; i++) {
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

                        sem_wait(semaforo_log);
                        char buffa[4096 * 4];
                        sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, token);
                        escreverLog(buffa);
                        sem_post(semaforo_log);

                        // Make the worker available again
                        sem_wait(semaforo);
                        sharedMemory->ocupados[id] = 0;
                        sem_post(semaforo);

                        continue;
                    }
                    // Clear the key queue
                    sem_wait(semaforo);

                    for (int i = 0; i < config.max_keys; i++) {
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

                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, token);
                    escreverLog(buffa);
                    sem_post(semaforo_log);

                } else if (strcmp(token, "sensors") == 0) {
                    // Print the key queus chave
                    sem_wait(semaforo);

                    for (int i = 0; i < config.max_keys; i++) {
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

                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, token);
                    escreverLog(buffa);
                    sem_post(semaforo_log);
                } else if (strncmp(token, "add_alert", strlen("add_alert")) == 0) {
                    // Separar o token em argumentos

                    char final[4096];
                    strcpy(final, token);

                    token = strtok(token, " ");
                    char *args[5];
                    int i = 0;

                    while (token != NULL) {
                        args[i] = token;
                        token = strtok(NULL, " ");
                        i++;
                    }

                    // Verificar tamanho da alerts queue
                    sem_wait(semaforo);

                    if (sharedMemory->alerts_size == config.max_alerts) {
                        strcpy(msg.content, "ERROR");

                        if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(1);
                        }

                        sem_wait(semaforo_log);
                        char buffa[4096 * 4];
                        sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, final);
                        escreverLog(buffa);
                        sem_post(semaforo_log);

                        sem_post(semaforo);

                        // Make the worker available again
                        sem_wait(semaforo);
                        sharedMemory->ocupados[id] = 0;
                        sem_post(semaforo);

                        continue;
                    }

                    sem_post(semaforo);

                    // Adicionar à alert queue
                    sem_wait(semaforo);

                    for (i = 0; i < config.max_alerts; i++) {
                        if (strcmp(sharedMemory->alert_queue[i].id, "") == 0) {
                            strcpy(sharedMemory->alert_queue[i].id, args[1]);
                            strcpy(sharedMemory->alert_queue[i].chave, args[2]);
                            sharedMemory->alert_queue[i].min = atoi(args[3]);
                            sharedMemory->alert_queue[i].max = atoi(args[4]);
                            sharedMemory->alert_queue[i].consoleID = atoi(consoleID);

                            if (i == 0) {
                                sem_post(semaforo_alerts);
                            }

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

                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, final);
                    escreverLog(buffa);
                    sem_post(semaforo_log);

                } else if (strcmp(token, "list_alerts") == 0) {
                    sem_wait(semaforo);
                    for (int i = 0; i < config.max_alerts; i++) {
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

                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, token);
                    escreverLog(buffa);
                    sem_post(semaforo_log);
                } else if (strncmp(token, "remove_alert", strlen("remove-alert")) == 0) {
                    char final[4096];
                    strcpy(final, token);

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

                    for (i = 0; i < config.max_alerts; i++) {
                        if (strcmp(sharedMemory->alert_queue[i].id, args[1]) == 0) {
                            strcpy(sharedMemory->alert_queue[i].id, "");
                            strcpy(sharedMemory->alert_queue[i].chave, "");
                            sharedMemory->alert_queue[i].min = 0;
                            sharedMemory->alert_queue[i].max = 0;
                            sharedMemory->alert_queue[i].consoleID = 0;

                            if (i == 0) {
                                sem_wait(semaforo_alerts);
                            }

                            break;
                        }
                    }

                    sem_post(semaforo);

                    sem_wait(semaforo);
                    sharedMemory->alerts_size--;
                    sem_post(semaforo);

                    Message msg;
                    msg.type = atoi(consoleID);

                    // Send the message to the console
                    strcpy(msg.content, "OK");

                    if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                        perror("msgsnd");
                        exit(1);
                    }

                    sem_wait(semaforo_log);
                    char buffa[4096 * 4];
                    sprintf(buffa, "WORKER%d: %s PROCESSING COMPLETED\n", id, final);
                    escreverLog(buffa);
                    sem_post(semaforo_log);
                }
            }

            // Make the worker available again
            sem_wait(semaforo);
            sharedMemory->ocupados[id] = 0;
            sem_post(semaforo);
        }
        memset(buffer, 0, sizeof(buffer));
        token = NULL;
        sem_post(semaforo);
    }
}

void AlertsWatcherProcess(int id) {
    int printed;

    sem_wait(semaforo_log);
    escreverLog("PROCESS ALERTS_WATCHER CREATED\n");
    sem_post(semaforo_log);

    while (1) {
        sem_wait(semaforo_alerts);

        for (int i = 0; i < config.max_alerts; i++) {
            if (strcmp(sharedMemory->alert_queue[i].id, "") != 0) {
                for (int j = 0; j < config.max_keys; j++) {
                    if (strcmp(sharedMemory->key_queue[j].chave, sharedMemory->alert_queue[i].chave) == 0) {
                        if (printed == sharedMemory->key_queue[j].last) {
                            continue;
                        }

                        if (sharedMemory->key_queue[j].last < sharedMemory->alert_queue[i].min || sharedMemory->key_queue[j].last > sharedMemory->alert_queue[i].max) {
                            Message msg;
                            msg.type = sharedMemory->alert_queue[i].consoleID;
                            sprintf(msg.content, "ALERT %s (%s %d to %d) TRIGGERED\n", sharedMemory->alert_queue[i].id, sharedMemory->key_queue[j].chave, sharedMemory->alert_queue[i].min, sharedMemory->alert_queue[i].max);

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

        sem_post(semaforo_alerts);
    }
}

// ================== Functions ==========================

void clearQueue() {
    Queue *currentNode = queue;

    while (currentNode != NULL) {
        Queue *aux = currentNode;
        currentNode = currentNode->next;
        free(aux);
    }

    queue = NULL;
}

void ignorar() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    for (int i = 1; i <= 64; i++) {
        if (i == SIGINT) {
            continue;
        }
        if (sigaction(i, &sa, NULL) == -1) {
            // Ignorar sinais que não são suportados
            if (errno == EINVAL) {
                continue;
            }
            perror("Erro a ignorar sinais");
            exit(1);
        }
    }
}

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
        escreverLog("SIGNAL SIGTSTP RECEIVED\n");
        escreverLog("HOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH\n");
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

        // Clear the internal queue
        clearQueue();

        sem_wait(semaforo_log);
        escreverLog("HOME_IOT SIMULATOR CLOSING\n");
        sem_post(semaforo_log);

        // Close the semaphores
        sem_close(semaforo);
        sem_unlink(SEM_NAME);
        sem_close(semaforo_log);
        sem_unlink(SEM_LOG_NAME);
        sem_close(semaforo_alerts);
        sem_unlink(SEM_ALERTS_NAME);

        pthread_mutex_destroy(&mutex);

        fclose(logFile);

        exit(0);
    } else {
        // Terminate the child process
        exit(0);
    }
}

void addNodeToQueue(Queue *newNode) {
    Queue *currentNode = queue;

    // If the queue is empty, add the node to the queue
    if (currentNode == NULL) {
        queue = newNode;
        return;
    }

    // If the queue is not empty, add the node to the queue. First the nodes with prioridade 9 and then the nodes with prioridade 1
    while (currentNode->next != NULL) {
        if (currentNode->next->prioridade < newNode->prioridade) {
            newNode->next = currentNode->next;
            currentNode->next = newNode;
            return;
        }
        currentNode = currentNode->next;
    }
}

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

    sem_wait(semaforo);
    for (int i = 0; i < config.num_workers; i++) {
        sharedMemory->ocupados[i] = 0;
    }
    sharedMemory->queue_size = 0;
    sharedMemory->key_size = 0;
    sharedMemory->alerts_size = 0;
    sem_post(semaforo);

    return 0;
}

void *sensorReader() {
    sem_wait(semaforo_log);
    escreverLog("THREAD SENSOR_READER\n");
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

            sem_wait(semaforo);
            // Check if queue is full
            if (sharedMemory->queue_size == config.queue_size) {
                sem_wait(semaforo_log);
                escreverLog("ERROR: QUEUE IS FULL\n");
                sem_post(semaforo_log);

                continue;
            } else {
                sharedMemory->queue_size++;
            }
            sem_post(semaforo);

            // Add the message to the queue
            Queue *newNode = (Queue *)malloc(sizeof(Queue));

            newNode->next = NULL;

            strcpy(newNode->ID, sensorID);
            strcpy(newNode->chave, key);
            newNode->valor = value;
            newNode->prioridade = 1;

            pthread_mutex_lock(&mutex);
            addNodeToQueue(newNode);
            pthread_mutex_unlock(&mutex);

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
    escreverLog("THREAD CONSOLE_READER CREATED\n");
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

            sem_wait(semaforo);
            // Check if queue is full
            if (sharedMemory->queue_size == config.queue_size) {
                sem_wait(semaforo_log);
                escreverLog("ERROR: QUEUE IS FULL\n");
                sem_post(semaforo_log);

                continue;
            } else {
                sharedMemory->queue_size++;
            }
            sem_post(semaforo);

            // Add the command to the queue
            Queue *newNode = (Queue *)malloc(sizeof(Queue));

            newNode->next = NULL;
            strcpy(newNode->command, command);
            newNode->prioridade = 9;

            pthread_mutex_lock(&mutex);
            addNodeToQueue(newNode);
            pthread_mutex_unlock(&mutex);

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
    escreverLog("THREAD DISPATCHER CREATED\n");
    sem_post(semaforo_log);

    // Get the argument fd_unnamed_pipe[config.num_workers][2]
    int(*fd_unnamed_pipe)[2] = (int(*)[2])arg;

    while (1) {
        // If the queue is not empty
        if (queue != NULL) {
            // Get the first node of the queue
            pthread_mutex_lock(&mutex);
            Queue *currentNode = popNodeFromQueue();
            sem_wait(semaforo);
            sharedMemory->queue_size--;
            sem_post(semaforo);
            pthread_mutex_unlock(&mutex);

            if (currentNode != NULL) {
                // If the node is a command
                if (currentNode->prioridade == 9) {
                    // Write to a random worker unnamed pipe
                    int randomWorker = rand() % config.num_workers;

                    // Check if the worker is busy
                    while (sharedMemory->ocupados[randomWorker] == 1) {
                        randomWorker = rand() % config.num_workers;
                    }

                    char message[BUFF_SIZE * 4];
                    sprintf(message, "C;%s", currentNode->command);

                    // Write to the worker
                    if (write(fd_unnamed_pipe[randomWorker][1], message, sizeof(currentNode->command)) < 0) {
                        perror("Error writing to worker");
                    }

                    // Write to the log
                    sem_wait(semaforo_log);
                    char buffa[4096 * 3];
                    // sprintf(buffa, "DISPATCHER: %s SENT FOR PROCESSING ON WORKER %d\n", currentNode->command, randomWorker);

                    // CurrentNode->command except last 2 characters
                    char command[64];
                    strncpy(command, currentNode->command, strlen(currentNode->command) - 2);
                    command[strlen(currentNode->command) - 2] = '\0';

                    sprintf(buffa, "DISPATCHER: %s SENT FOR PROCESSING ON WORKER %d\n", command, randomWorker);
                    escreverLog(buffa);
                    sem_post(semaforo_log);
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

                    sem_wait(semaforo_log);
                    char buffa[4096 * 3];
                    sprintf(buffa, "DISPATCHER: %s %s %d SENT FOR PROCESSING ON WORKER %d\n", currentNode->ID, currentNode->chave, currentNode->valor, randomWorker);
                    escreverLog(buffa);
                    sem_post(semaforo_log);
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
    ignorar();

    // ===================================== Create the Semaphores ==========================================
    sem_unlink(SEM_NAME);
    if ((semaforo = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) {
        perror("Error creating shm semaphore");
        terminateAll();
    }

    sem_unlink(SEM_LOG_NAME);
    if ((semaforo_log = sem_open(SEM_LOG_NAME, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) {
        perror("Error creating log semaphore");
        terminateAll();
    }

    sem_unlink(SEM_ALERTS_NAME);
    if ((semaforo_alerts = sem_open(SEM_ALERTS_NAME, O_CREAT | O_EXCL, 0777, 0)) == SEM_FAILED) {
        perror("Error creating alerts semaphore");
        terminateAll();
    }

    sem_unlink(SEM_KEY_NAME);
    if ((semaforo_keys = sem_open(SEM_KEY_NAME, O_CREAT | O_EXCL, 0777, 0)) == SEM_FAILED) {
        perror("Error creating keys semaphore");
        terminateAll();
    }

    sem_wait(semaforo_log);
    escreverLog("HOME_IOT SIMULATOR STARTING\n");
    sem_post(semaforo_log);

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