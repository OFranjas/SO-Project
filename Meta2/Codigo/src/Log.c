#include "header.h"

// void escreverLog(char *mensagem) {
//     // sem_wait(semaforo_log);

//     FILE *logFile;

//     if ((logFile = fopen("log.txt", "a")) == NULL) {
//         printf("Error opening file\n");
//         return;
//     }

//     time_t my_time;
//     struct tm *timeInfo;
//     time(&my_time);
//     timeInfo = localtime(&my_time);

//     // Write the message to the log file with the date before with the format: "HH:MM:SS MESSAGE"
//     fprintf(logFile, "%d:%d:%d -> %s\n", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec, mensagem);

//     // Print the message to the console
//     printf("%d:%d:%d -> %s\n", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec, mensagem);

//     fclose(logFile);

//     // sem_post(semaforo_log);
// }