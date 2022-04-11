// José Miguel Saraiva Monteiro - 2015235572

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
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
#define MUTEX_LOGGER "MUTEX_LOGGER"
#define MUTEX_CONFIG "MUTEX_CONFIG"
#define MUTEX_SERVERS "MUTEX_SERVERS"
#define MUTEX_STATS "MUTEX_STATS"

// Variables Below
int shmid;
int fd_task_pipe;
sem_t *mutex_config, *mutex_servers, *mutex_stats;
key_t shmkey;
prog_config *program_configuration;
edge_server *servers;
statistics *program_stats;


int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Bad command\n");
        printf("Correct command usage:\n$ offload_simulator {ficheiro_configuração}\n");
        exit(-1);
    }

    // TODO: Semaphores / MUTEXES / Condition variables
    
    // ? Start of Semaphores / Mutexes
    
    start_semaphores();
    
    // ? End of Semaphores / Mutexes

    // * Keep this call after mutexes startup to avoid errors 
    // * as the logger uses one of the mutexes
    handle_log("INFO: Offload Simulator Starting");

    load_config(argv[1]);
    
    // ! Pipe can only be created BEFORE the creation of child processes
    // TODO: [Final] Create "named pipe" => TASK_PIPE 
    if (mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0777) < 0) {
        perror("mkfifo() error | Error creating named pipe!\n");
        exit(0);
    }

    // Open pipe for read + write
    fd_task_pipe = open(PIPE_NAME, O_RDWR);
    if (fd_task_pipe < 0) {
        perror("open(PIPE) error\n");
        exit(0);
    }

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

    // ! IMPROVE LATER
    // signal(SIGINT, handle_program_finish);
    
    handle_program_finish();
    // while(1) {}
    
    return 0;
}

void start_semaphores() {
    sem_unlink(MUTEX_LOGGER);
    if ((mutex_logger = sem_open(MUTEX_LOGGER, O_CREAT | O_EXCL, 0777, 1)) < 0) {
        perror("sem_open() Error - MUTEX_LOGGER\n");
        exit(-1);
    }

    sem_unlink(MUTEX_CONFIG);
    if ((mutex_config = sem_open(MUTEX_CONFIG, O_CREAT | O_EXCL, 0777, 1)) < 0) {
        perror("sem_open() Error - MUTEX_CONFIG\n");
        exit(-1);
    }
    
    sem_unlink(MUTEX_SERVERS);
    if ((mutex_servers = sem_open(MUTEX_SERVERS, O_CREAT | O_EXCL, 0777, 1)) < 0) {
        perror("sem_open() Error - MUTEX_SERVERS\n");
        exit(-1);
    }
    
    sem_unlink(MUTEX_STATS);
    if ((mutex_stats = sem_open(MUTEX_STATS, O_CREAT | O_EXCL, 0777, 1)) < 0) {
        perror("sem_open() Error - MUTEX_STATS\n");
        exit(-1);
    }
}

void handle_program_finish(int signum) {
    // * Wait for processes [ Maintenance Manager, Monitor, Task Manager ]
    wait(NULL);
    wait(NULL);
    wait(NULL);

    #ifdef DEBUG
    printf("Starting to cleanup resources!\n");
    #endif

    handle_log("INFO: Simulator Closing!");

    // ? Maybe use waitpid later ?
    // waitpid(program_configuration->monitor_pid, NULL, 0);
    // waitpid(program_configuration->task_manager_pid, NULL, 0);
    // waitpid(program_configuration->maintenance_monitor_pid, NULL, 0);

    #ifdef DEBUG
    printf("\nSystem mngr PID =>%ld\nMonitor PID => %ld\nTask mngr PID => %ld\nMaintenance mngr PID => %ld\n\n", 
                                                (long)getpid(),
                                                (long)program_configuration->monitor_pid,
                                                (long)program_configuration->task_manager_pid,
                                                (long)program_configuration->maintenance_monitor_pid);
    #endif

    // * Unlink semaphores
    sem_unlink(MUTEX_LOGGER);
    sem_unlink(MUTEX_CONFIG);
    sem_unlink(MUTEX_SERVERS);
    sem_unlink(MUTEX_STATS);

    // * Close pipe
    close(fd_task_pipe);
    unlink(PIPE_NAME);

    // * Clean memory resources
    shmdt(program_configuration);
    shmctl(shmid, IPC_RMID, NULL);

    #ifdef DEBUG
    printf("Resources cleaned, exiting!\n");
    #endif

    exit(0);
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
    
    // ? Note to self: Do I really need this?
    // Copied from factory_main.c PL4 Ex5
    if ((shmkey = ftok(".", getpid())) == (key_t) -1){
        perror("IPC error: ftok\n");
        exit(1);
    };
    // End of copied code

    shmid = shmget(shmkey, sizeof(prog_config) 
                        + (sizeof(edge_server) * edge_server_number) 
                        + sizeof(statistics)
                        , IPC_CREAT | 0777);
    if (shmid < 1) {
        perror("Error Creating shared memory\n");
        exit(1);
    }

    program_configuration = (prog_config *) shmat(shmid, NULL, 0);

    // Lock config SHM area
    sem_wait(mutex_config);

    program_configuration->queue_pos = queue_pos;
    program_configuration->max_wait = max_wait;
    program_configuration->edge_server_number = edge_server_number;

    // Unlock config memory area
    sem_post(mutex_config);

    if (program_configuration < (prog_config*) 1) {
        perror("Error attaching memory\n");
        exit(1);
    }

    servers = (edge_server *) (program_configuration + 1);
    program_stats = (statistics *) (servers + 1);

    // Lock servers SHM area
    sem_wait(mutex_servers);
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
    // Unlock Servers SHM area
    sem_post(mutex_servers);

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

