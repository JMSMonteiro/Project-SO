#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include "system_manager.h"
#include "logger.h"
#include "task_manager.h"


// Defines Below 
// #define DEBUG   // Remove/Comment this line to remove debug messages

// Variables Below
prog_config *program_configuration;


int main() {
    handle_log("INFO: Offload Simulator Starting");

    load_config();

    // TODO: [Final] Create "named pipe" => TASK_PIPE 

    // fork() == 0 => Child process 
    if (fork() == 0) {
        // ! Important
        // TODO: [Intermediate] Create process <=> Task Manager
        handle_log("INFO: Creating Process: 'Task Manager'");
        task_manager(program_configuration);
        exit(0);
    }

    if (fork() == 0) {
        // ! Important
        // TODO: [Intermediate] Create process <=> Monitor 
        handle_log("INFO: Creating Process: 'Monitor'");
        exit(0);
    }

    if (fork() == 0) {
        // ! Important
        // TODO: [Intermediate] Create process <=> Maintenance Manager 
        handle_log("INFO: Creating Process: 'Maintenance Manager'");
        exit(0);
    }


    // TODO: [Final] Create Message Queue

    // ! Important
    // TODO: [Intermediate] Create Shared Memory

    // TODO: [Final] Catch SIGSTP & Print Stats

    // TODO: [Final] Catch SIGINT to finish program

    // ? Insert monitor call here ?

    // ! REMOVE LATER
    wait(NULL);
    wait(NULL);
    wait(NULL);
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

    if (program_configuration->edge_server_number < 2) {
        handle_log("ERROR: Config file - Edge Server number must be >= 2");
        exit(-1);
    }

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

