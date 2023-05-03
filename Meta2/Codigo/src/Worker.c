#include "lib/header.h"

// void WorkerProcess(int id, int fd) {
//     // Create String with the process ID
//     char buf[BUFF_SIZE * 10];
//     sprintf(buf, "Worker process ID %d created\n", id);

//     escreverLog(buf);

//     while (1) {
//         // Read from the pipe
//         char buffer[BUFF_SIZE * 4];

//         int r = read(fd, buffer, sizeof(buffer));

//         if (r == -1) {
//             perror("read");
//             exit(1);
//         }

//         if (r > 1) {
//             // Print the message
//             sprintf(buf, "Worker %d received: %s\n", id, buffer);
//             escreverLog(buf);

//             // Parse the message to check if it's from a sensor or a console
//             char *token = strtok(buffer, ";");

//             if (strcmp(token,"S") == 0){

//                 // Parse the message ID#key#value
//                 token = strtok(NULL, ";");

//                 char *sensorID = strtok(token, "#");
//                 char *key = strtok(NULL, "#");
//                 char *value = strtok(NULL, "#");

//                 // Add to the key queue

                


//             }
//         }
//     }
// }