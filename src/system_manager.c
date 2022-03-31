// José Miguel Saraiva Monteiro - 2015235572

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <wait.h>
#include <semaphore.h>
#include <fcntl.h>

#include "system_manager.h"
#include "logger.h"
#include "task_manager.h"
#include "monitor.h"
#include "maintenance_manager.h"


// Defines Below 

// Variables Below
int shmid;
key_t shmkey;
prog_config *program_configuration;
edge_server *servers;


int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Bad command\n");
        printf("Correct command usage:\n$ offload_simulator {ficheiro_configuração}\n");
        exit(-1);
    }

    // TODO: Semaphores / MUTEXES / Condition variables
    sem_unlink("MUTEX_LOGGER");
    if ((mutex_logger = sem_open("MUTEX_LOGGER", O_CREAT | O_EXCL, 0777, 1)) < 0) {
        perror("sem_open() Error - MUTEX_LOGGER\n");
        exit(-1);
    }

    handle_log("INFO: Offload Simulator Starting");

    load_config(argv[1]);

    // TODO: [Final] Create "named pipe" => TASK_PIPE 

    // fork() == 0 => Child process 
    if (fork() == 0) {
        // ! Important
        // TODO: [Intermediate] Create process <=> Task Manager
        handle_log("INFO: Creating Process: 'Task Manager'");
        task_manager();
        exit(0);
    }

    if (fork() == 0) {
        // ! Important
        // TODO: [Intermediate] Create process <=> Monitor 
        handle_log("INFO: Creating Process: 'Monitor'");
        monitor();
        exit(0);
    }

    if (fork() == 0) {
        // ! Important
        // TODO: [Intermediate] Create process <=> Maintenance Manager 
        handle_log("INFO: Creating Process: 'Maintenance Manager'");
        maintenance_manager();
        exit(0);
    }


    // TODO: [Final] Create Message Queue

    // TODO: [Final] Catch SIGSTP & Print Stats

    // TODO: [Final] Catch SIGINT to finish program

    // ? Insert monitor call here ?

    // ! REMOVE LATER
    wait(NULL);
    wait(NULL);
    wait(NULL);

    // Clean memory resources
    shmdt(program_configuration);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}

void load_config(char *file_name) {
    FILE *qPtr;
    int i = 0;
    int validate_config = 0;
    char line[64];
    char *word = NULL;
    int queue_pos, max_wait, edge_server_number;
    
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
        exit(-1);
    }

    // Read info from file
    validate_config += fscanf(qPtr, "%d\n", &queue_pos);
    validate_config += fscanf(qPtr, "%d\n", &max_wait);
    validate_config += fscanf(qPtr, "%d\n", &edge_server_number);

    if (validate_config < 3) {
        perror("ERROR: Config file - Invalid value in {QUEUE_POS} | {MAX_WAIT} | {SERVER_NUMBER}. Check first three lines\n");
        exit(-1);
    }

    if (edge_server_number < 2) {
        handle_log("ERROR: Config file - Edge Server number must be >= 2");
        exit(-1);
    }

    // ! Important
    // TODO: [Intermediate] Create Shared Memory
    /*
    * In struct prog_config, remove the field edge_server *servers
    * Instead, create shared memory with: 
    * size = sizeof(prog_config) + server_number * sizeof(edge_server) + {Insert aditional stuff needed}
    */
    
    // Copied from factory_main.c PL4 Ex5
    if ((shmkey = ftok(".", getpid())) == (key_t) -1){
        perror("IPC error: ftok\n");
        exit(1);
    };
    // End of copied code

    shmid = shmget(shmkey, sizeof(prog_config) + sizeof(edge_server) * edge_server_number, IPC_CREAT | 0777);
    if (shmid < 1) {
        perror("Error Creating shared memory\n");
        exit(1);
    }

    program_configuration = (prog_config *) shmat(shmid, NULL, 0);

    program_configuration->queue_pos = queue_pos;
    program_configuration->max_wait = max_wait;
    program_configuration->edge_server_number = edge_server_number;

    if (program_configuration < (prog_config*) 1) {
        perror("Error attaching memory\n");
        exit(1);
    }

    servers = (edge_server *) (program_configuration + 1);

    for (i = 0; i < program_configuration->edge_server_number; i++) {
        if (fgets(line, sizeof(line), qPtr) == NULL) {
            perror("Reached EOF\n");
        }
        // Read each line and divide it by the comma ","
        word = strtok(line, ",");
        // Copy first value (name) into it's respective var
        strcpy(servers[i].name, word);
        // Get the second value (vCPU1)
        word = strtok(NULL, ",");
        // Assign vCPU1 to it's variable
        servers[i].v_cpu1 = atoi(word);
        
        // Get the third value (vCPU2)
        word = strtok(NULL, ",");
        // Assign vCPU2 to it's variable
        servers[i].v_cpu2 = atoi(word);

        if (servers[i].v_cpu1 == 0 
            || servers[i].v_cpu2 == 0 ) {
            handle_log("WARNING: Possible error in Server Configs");
            }
    }

    #ifdef DEBUG
    printf("QUEUE_POS = %d\nMAX_WAIT = %d\nSERVER_NUMBER = %d\n", 
                                            queue_pos, 
                                            max_wait, 
                                            edge_server_number);

    for (i = 0; i < edge_server_number; i++) {
        printf("%s,%d,%d\n", servers[i].name, 
                                servers[i].v_cpu1, 
                                servers[i].v_cpu2);
    }

    printf("End of load_config()\n");
    #endif

    fclose(qPtr);
}

