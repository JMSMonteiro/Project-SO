#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include "main.h"
#include "logger.h"

// Defines Below 
#define DEBUG   // Remove/Comment this line to remove debug messages

// Variables Below
prog_config *program_configuration;


int main() {
    handle_log("Offload Simulator Starting");

    load_config();

    // handle_log("THIS IS A TEST");
    
    return 0;
}

void load_config() {
    FILE *qPtr;
    int i;
    char *file_name = "config.txt";
    char line[64];
    char *word = NULL;

    /*
    * CONFIG INFO
    ? QUEUE_POS - número de slots na fila interna do Task Manager
    ? MAX_WAIT - tempo máximo para que o processo Monitor eleve o nível de performance dos Edge Servers (em segundos)
    ? EDGE_SERVER_NUMBER - número de edge servers (>=2)
    ? Nome do servidor de edge e capacidade de processamento de cada vCPU em MIPS
    */
    #ifdef DEBUG
    printf("Start of load_config()\n");
    #endif

    // Open file
    if ((qPtr = fopen(file_name, "r")) == 0) {
        perror("Error opening Config file\n");
    }

    // Allocate memory for config struct
    program_configuration = malloc(sizeof(prog_config));

    // Read info from file
    fscanf(qPtr, "%d\n", &program_configuration->queue_pos);
    fscanf(qPtr, "%d\n", &program_configuration->max_wait);
    fscanf(qPtr, "%d\n", &program_configuration->edge_server_number);

    // Allocate memory for servers inside 
    program_configuration->servers = malloc(program_configuration->edge_server_number * sizeof(edge_server));

    for (i = 0; i < program_configuration->edge_server_number; i++) {
        if (fgets(line, sizeof(line), qPtr) == NULL) {
            perror("Reached EOF\n");
        }
        // Read each line and divide it by the comma ","
        word = strtok(line, ",");
        // Copy first value (name) into it's respective var
        strcpy(program_configuration->servers[i].name, word);
        // Get the second value (vCPU1)
        word = strtok(NULL, ",");
        // Assign vCPU1 to it's variable
        program_configuration->servers[i].v_cpu1 = atoi(word);
        
        // Get the third value (vCPU2)
        word = strtok(NULL, ",");
        // Assign vCPU2 to it's variable
        program_configuration->servers[i].v_cpu2 = atoi(word);
    }

    #ifdef DEBUG
    printf("QUEUE_POS = %d\nMAX_WAIT = %d\nSERVER_NUMBER = %d\n", 
                                            program_configuration->queue_pos, 
                                            program_configuration->max_wait, 
                                            program_configuration->edge_server_number);

    for (i = 0; i < program_configuration->edge_server_number; i++) {
        printf("%s,%d,%d\n", program_configuration->servers[i].name, 
                                    program_configuration->servers[i].v_cpu1, 
                                    program_configuration->servers[i].v_cpu2);
    }

    printf("End of load_config()\n");
    #endif

    fclose(qPtr);
}

