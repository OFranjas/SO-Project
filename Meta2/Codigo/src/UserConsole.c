#include "header.h"
/*
- Menu para ler comandos
*/

int fd_console_pipe;

int debug = 0;

int msgqid;

int consoleID;

int addAlert(char *alertId, char *key, int min, int max) {
    // Verificar se o sensor_id é válido (minimo 3 caracteres, maximo 32)
    if (strlen(alertId) < MIN_STRING_SIZE || strlen(alertId) > MAX_STRING_SIZE) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    // Verificar se a chave é válida (minimo 3 caracteres, maximo 32) formada por uma combinação de digitos, letras e _
    if (strlen(key) < MIN_STRING_SIZE || strlen(key) > MAX_STRING_SIZE) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    // Verificar se valor minimo é válido (>=0)
    if (min < 0) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    // Verificar se valor maximo é válido (>=0)
    if (max < 0) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    if (debug)
        printf("add_alert %s %s %d %d\n", alertId, key, min, max);

    return 0;
}

void *reader() {
    Message buffer;

    while (1) {
        // Ler da Message Queue
        if (msgrcv(msgqid, &buffer, sizeof(buffer), consoleID, 0) < 0) {
            perror("Error reading from Message Queue\n");
            return NULL;
        }
        // Escrever no ecrã
        printf("%s\n", buffer.content);
    }

    pthread_exit(NULL);
}

void ignorar() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    for (int i = 1; i <= 64; i++) {
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

int main(int argc, char *argv[]) {
    // =================== Receber o ID da consola como parâmetro ===================
    if (argc != 2) {
        printf("Número de argumentos inválido. Tente novamente.\n");
        return 1;
    }

    if (atoi(argv[1]) < 0) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    consoleID = atoi(argv[1]);

    ignorar();

    // printf("Console ID: %d\n", consoleID);

    // =================== Abrir Named Pipe ===================

    if ((fd_console_pipe = open("consolePipe", O_WRONLY | O_NONBLOCK)) < 0) {
        perror("Error opening Console Pipe\n");
        return 1;
    }

    // =================== Abrir Message Queue ===================

    key_t key = ftok(".", MSQ_KEY);

    if ((msgqid = msgget(key, 0660)) < 0) {
        perror("Error opening Message Queue\n");
        return 1;
    }
    
    // =================== Criar thread para ler da Message Queue ===================
    pthread_t reader_thread;

    if (pthread_create(&reader_thread, NULL, reader, NULL) != 0) {
        perror("Error creating reader thread\n");
        return 1;
    }

    // =================== Ler comandos ===================
    char command[1024];
    char *token;
    char *args[64];
    int i = 0;
    char aux[4096];

    while (1) {
        printf(">> ");

        // Ler comando
        fgets(command, 64, stdin);

        // Remover o \n do final da string
        command[strlen(command) - 1] = '\0';

        // Verificar todos os possíveis comandos (exit, stats, reset, sensors, list_alerts) or add_alert <sensor_id> <key> <min> <max> or remove_alert <sensor_id>
        if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");

            pthread_cancel(reader_thread);

            break;
        } else if (strcmp(command, "stats") == 0) {
            printf("Key Last Min Max Avg Count\n");

            sprintf(aux, "stats;%d", consoleID);

            write(fd_console_pipe, aux, strlen(aux));

        } else if (strcmp(command, "reset") == 0) {
            sprintf(aux, "reset;%d", consoleID);
            write(fd_console_pipe, aux, strlen(aux));

        } else if (strcmp(command, "sensors") == 0) {
            sprintf(aux, "sensors;%d", consoleID);
            write(fd_console_pipe, aux, strlen(aux));

        } else if (strcmp(command, "list_alerts") == 0) {
            printf("ID Key MIN MAX\n");

            sprintf(aux, "list_alerts;%d", consoleID);

            write(fd_console_pipe, aux, strlen(aux));

        } else if (strncmp(command, "add_alert", 9) == 0) {
            strcpy(aux, command);

            // Separar o input em tokens
            token = strtok(aux, " ");

            i = 0;

            while (token != NULL) {
                args[i] = token;
                token = strtok(NULL, " ");
                i++;
            }

            // Verificar se o número de argumentos é válido
            if (i != 5) {
                printf("Número de argumentos inválido. Tente novamente.\n");
                continue;
            }

            if (addAlert(args[1], args[2], atoi(args[3]), atoi(args[4]))) {
                continue;
            }

            sprintf(aux, "%s;%d", command, consoleID);

            // printf("Aux: %s\n", aux);

            write(fd_console_pipe, aux, strlen(aux));

        } else if (strncmp(command, "remove_alert", 12) == 0) {
            strcpy(aux, command);

            // Separar o input em tokens
            token = strtok(aux, " ");

            i = 0;

            while (token != NULL) {
                args[i] = token;
                token = strtok(NULL, " ");
                i++;
            }

            // Verificar se o número de argumentos é válido
            if (i != 2) {
                printf("Número de argumentos inválido. Tente novamente.\n");
                continue;
            }

            // Verificar se o sensor_id é válido (minimo 3 caracteres, maximo 32)
            if (strlen(args[1]) < MIN_STRING_SIZE || strlen(args[1]) > MAX_STRING_SIZE) {
                printf("Argumentos inválidos. Tente novamente.\n");
                continue;
            }

            char *alertID = args[1];
            if (debug)
                printf("remove_alert %s\n", alertID);

            // * TODO Fazer o suposto

            sprintf(aux, "%s;%d", command, consoleID);

            write(fd_console_pipe, aux, strlen(aux));

        } else {
            printf("Comando inválido. Tente novamente.\n");
        }

        bzero(command, sizeof(command));

        bzero(aux, sizeof(aux));

        bzero(args, sizeof(args));
    }

    return 0;
}