#include "header.h"

/*
- Ler os comandos da linha de comandos e validar (IDs têm de ser strings e têm tamanho específico)
- Meter tudo dentro de uma string (Como no enunciado)

*/

int debug = 1;

int fd_sensor_pipe;

int counter = 0;

void enviadas() {
    printf("Enviadas %d mensagens\n", counter);
}

void stop() {
    printf("Encerrando sensor...\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    // Verificar se o número de argumentos é válido
    if (argc != 6) {
        printf("Número de argumentos inválido. Tente novamente.\n");
        return 1;
    }

    signal(SIGTSTP, enviadas);
    signal(SIGINT, stop);

    // ================== Abrir Named Pipe ==========================
    if ((fd_sensor_pipe = open("sensorPipe", O_WRONLY)) < 0) {
        perror("Error opening sensor_pipe");
    }

    // ================== Verificar Argumentos ==========================

    //  Verificar Identificador do Sensor (minimo 3 caracteres, maximo 32)
    if (strlen(argv[1]) < 3 || strlen(argv[1]) > 32) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    char *sensorId = argv[1];

    // Verificar se o intervalo é válido (>=0)
    if (atoi(argv[2]) < 0) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    int interval = atoi(argv[2]);

    // Verificar se a chave é válida (minimo 3 caracteres, maximo 32) formada por uma combinação de digitos, letras e _
    if (strlen(argv[3]) < 3 || strlen(argv[3]) > 32) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    char *key = argv[3];

    // Verificar se valor minimo é válido (>=0)
    if (atoi(argv[4]) < 0) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    int min = atoi(argv[4]);

    // Verificar se valor maximo é válido (>=0)
    if (atoi(argv[5]) < 0) {
        printf("Argumentos inválidos. Tente novamente.\n");
        return 1;
    }

    int max = atoi(argv[5]);

    // ================== Gerar Mensagens ==========================

    int random;

    while (1) {
        random = rand() % (max)-min + 1 + min;

        // Gerar a string com os argumentos
        char *string = malloc(sizeof(char) * 100);
        sprintf(string, "%s#%s#%d", sensorId, key, random);

        // Enviar a string para a fila de mensagens
        write(fd_sensor_pipe, string, strlen(string));

        counter++;

        // Esperar intervalo de tempo
        if (debug)
            printf("Aguardando %d segundos para enviar a próxima mensagem...\n", interval);

        sleep(interval);
    }

    return 0;
}