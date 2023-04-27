#include "lib/header.h"

Config config;

int temp;

int readConfigFile(char *configFilePath) {
    FILE *configFile;

    if ((configFile = fopen(configFilePath, "r")) == NULL) {
        escreverLog("Error opening file\n");
        return 1;
    }

    // if (debug)
    //     escreverLog("File opened successfully\n");

    char line[100];
    char *token;

    // Read the first line
    if (fgets(line, sizeof(line) - 1, configFile)) {
        // Get value of QUEUE_SZ
        token = strtok(line, " ");
        temp = atoi(token);

        // Check if the value is valid
        if (temp < 1) {
            escreverLog("Invalid value for QUEUE_SZ - Must be >= 1\n");
            fclose(configFile);
            return 1;
        }

        config.queue_size = temp;

    } else {
        escreverLog("Error reading file - Bad format\n");
        fclose(configFile);
        return 1;
    }

    // Read the second line
    if (fgets(line, 100, configFile)) {
        // Get value of N_WORKERS
        token = strtok(line, " ");
        temp = atoi(token);

        // Check if the value is valid
        if (temp < 1) {
            escreverLog("Invalid value for N_WORKERS - Must be >= 1\n");
            fclose(configFile);
            return 1;
        }

        config.num_workers = temp;

    } else {
        escreverLog("Error reading file - Bad format\n");
        fclose(configFile);
        return 1;
    }

    // Read the third line
    if (fgets(line, 100, configFile)) {
        // Get value of MAX_KEYS
        token = strtok(line, " ");
        temp = atoi(token);

        // Check if the value is valid
        if (temp < 1) {
            escreverLog("Invalid value for MAX_KEYS - Must be >= 1\n");
            fclose(configFile);
            return 1;
        }

        config.max_keys = temp;

    } else {
        escreverLog("Error reading file - Bad format\n");
        fclose(configFile);
        return 1;
    }

    // Read the fourth line
    if (fgets(line, 100, configFile)) {
        // Get value of MAX_SENSORS
        token = strtok(line, " ");
        temp = atoi(token);

        // Check if the value is valid
        if (temp < 1) {
            escreverLog("Invalid value for MAX_SENSORS - Must be >= 1\n");
            fclose(configFile);
            return 1;
        }

        config.max_sensors = temp;

    } else {
        escreverLog("Error reading file - Bad format\n");
        fclose(configFile);
        return 1;
    }

    // Read the fifth line
    if (fgets(line, 100, configFile)) {
        // Get value of MAX_ALERTS
        token = strtok(line, " ");
        temp = atoi(token);

        // Check if the value is valid
        if (temp < 0) {
            escreverLog("Invalid value for MAX_ALERTS - Must be >= 0\n");
            fclose(configFile);
            return 1;
        }

        config.max_alerts = temp;

    } else {
        escreverLog("Error reading file - Bad format\n");
        fclose(configFile);
        return 1;
    }

    // Check if there are more lines
    if (fgets(line, 100, configFile)) {
        escreverLog("Error reading file - Bad format\n");
        fclose(configFile);
        return 1;
    }

    if (debug) {
        char buf[100];
        sprintf(buf, "Config file read QUEUE_SZ: %d | N_WORKERS: %d | MAX_KEYS: %d | MAX_SENSORS: %d | MAX_ALERTS: %d\n", config.queue_size, config.num_workers, config.max_keys, config.max_sensors, config.max_alerts);
        escreverLog(buf);
    }

    // Close the file
    fclose(configFile);

    return 0;
}