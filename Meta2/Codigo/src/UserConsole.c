#include "lib/header.h"
/*
- Menu para ler comandos
*/

// Global global;

int fd_console_pipe;

int debug = 1;

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

    int consoleID = atoi(argv[1]);

    printf("Console ID: %d\n", consoleID);

    // =================== Abrir Named Pipe ===================

    if ((fd_console_pipe = open("consolePipe", O_WRONLY | O_NONBLOCK)) < 0) {
        escreverLog("Error opening Console Pipe\n");
        return 1;
    }

    // =================== Ler comandos ===================
    char command[64];
    char *token;
    char *args[64];
    int i = 0;

    while (1) {
        printf(">> ");

        // Ler comando
        fgets(command, 64, stdin);

        // Remover o \n do final da string
        command[strlen(command) - 1] = '\0';

        // Verificar todos os possíveis comandos (exit, stats, reset, sensors, list_alerts) or add_alert <sensor_id> <key> <min> <max> or remove_alert <sensor_id>
        if (strcmp(command, "exit") == 0) {
            printf("Exiting...\n");

            break;
        } else if (strcmp(command, "stats") == 0) {
            printf("Key Last Min Max Avg Count\n");

            write(fd_console_pipe, "stats", sizeof("stats"));

        } else if (strcmp(command, "reset") == 0) {
            write(fd_console_pipe, "reset", sizeof("reset"));

        } else if (strcmp(command, "sensors") == 0) {
            write(fd_console_pipe, "sensors", sizeof("sensors"));

        } else if (strcmp(command, "list_alerts") == 0) {
            printf("ID Key MIN MAX\n");

            write(fd_console_pipe, "list_alerts", sizeof("list_alerts"));

        } else if (strncmp(command, "add_alert", 9) == 0) {
            char aux[64];
            strcpy(aux, command);

            // Separar o input em tokens
            token = strtok(command, " ");

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

            write(fd_console_pipe, aux, sizeof(aux));

        } else if (strncmp(command, "remove_alert", 12) == 0) {
            char aux[64];
            strcpy(aux, command);

            // Separar o input em tokens
            token = strtok(command, " ");

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

            write(fd_console_pipe, aux, sizeof(aux));

        } else {
            printf("Comando inválido. Tente novamente.\n");
        }
    }

    return 0;
}