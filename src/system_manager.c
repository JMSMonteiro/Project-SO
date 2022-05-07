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
#include <sys/msg.h>
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
int message_queue_id;
sem_t *mutex_config, *mutex_servers, *mutex_stats;
key_t shmkey;
prog_config *program_configuration;
edge_server *servers;
statistics *program_stats;


int main(int argc, char* argv[]) {
    sigset_t blocked_signals;
    
    if (argc != 2) {
        printf("Bad command\n");
        printf("Correct command usage:\n$ offload_simulator {ficheiro_configuração}\n");
        exit(-1);
    }

    // ? Start of signal Blocking
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    sigfillset(&blocked_signals);
    sigdelset(&blocked_signals, SIGINT);
    sigdelset(&blocked_signals, SIGTSTP);
    sigdelset(&blocked_signals, SIGUSR1);
    sigdelset(&blocked_signals, SIGUSR2);

    sigprocmask(SIG_SETMASK, &blocked_signals, NULL);

    // ? End of signal Blocking

    // ? Start of Semaphores / Mutexes
    
    start_semaphores();
    
    // ? End of Semaphores / Mutexes

    // * Keep this call after mutexes startup to avoid errors 
    // * as the logger uses one of the mutexes
    handle_log("INFO: Offload Simulator Starting");

    load_config(argv[1]);

    #ifdef DEBUG
    printf("Creating Pipe!\n");
    #endif
    
    // ! Pipe can only be created BEFORE the creation of child processes
    if (mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0777) < 0) {
        perror("mkfifo() error | Error creating named pipe!\n");
        exit(0);
    }

    // * Open pipe for read + write
    fd_task_pipe = open(PIPE_NAME, O_RDWR);
    if (fd_task_pipe < 0) {
        perror("open(PIPE) error\n");
        exit(0);
    }

    // * Create Message Queue
    message_queue_id = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
    if (message_queue_id < 0) {
        perror("msgget() error\n");
        exit(0);
    }

    #ifdef DEBUG
    printf("Pipe created!\n");
    #endif

    // fork() == 0 => Child process 
    if (fork() == 0) {
        handle_log("INFO: Creating Process: 'Task Manager'");
        task_manager();
    }

    if (fork() == 0) {
        handle_log("INFO: Creating Process: 'Monitor'");
        monitor();
    }

    if (fork() == 0) {
        handle_log("INFO: Creating Process: 'Maintenance Manager'");
        maintenance_manager();
    }

    // ? Insert monitor call here ?

    signal_initializer();

    while(1) {}
    
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

void signal_initializer() {
    // Capture SIGINT <=> Terminate Program
    signal(SIGINT, start_shutdown_routine);  // CTRL^C
    
    // Capture signal from Task Manager
    signal(SIGUSR1, handle_program_finish);  // Task Manager => "EXIT" > TASK_PIPE

    // Capture SIGTSTP <=> Print stats
    signal(SIGTSTP, handle_display_stats);  // CTRL^Z

    // Capture signal from Task Manager
    signal(SIGUSR2, display_stats);  // Task Manager => "STATS" > TASK_PIPE
}

void handle_display_stats(int signum) {
    handle_log("SIGNAL: Received SIGTSTP");

    display_stats(signum);
}

void display_stats(int signum) {
    char stats_message[LOG_MESSAGE_SIZE];
    int i;

    sem_wait(mutex_stats);

    // *Total # of performed tasks
    snprintf(stats_message, LOG_MESSAGE_SIZE, "STATS: %d Tasks executed!", program_stats->total_tasks_executed);
    handle_log(stats_message);
    
    // * AVG response time to each task (time from arrival to execution)
    snprintf(stats_message, LOG_MESSAGE_SIZE, "STATS: Average response time was: %d ms.", program_stats->avg_response_time);
    handle_log(stats_message);
    
    sem_wait(mutex_servers);
    for (i = 0; i < program_configuration->edge_server_number; i++) {
        // Display the name of the server
        snprintf(stats_message, LOG_MESSAGE_SIZE, "STATS: Statistics for server \'%s\':", servers[i].name);
        handle_log(stats_message);
        
        // * # tasks executed in each server
        snprintf(stats_message, LOG_MESSAGE_SIZE, "STATS: Tasks executed by server: %d !", servers[i].tasks_executed);
        handle_log(stats_message);

        // * # maintenante ops for each server
        snprintf(stats_message, LOG_MESSAGE_SIZE, "STATS: Maintenance operations done by the server: %d !", servers[i].maintenance_operation_performed);
        handle_log(stats_message);
    }
    sem_post(mutex_servers);

    // * # tasks not executed
    snprintf(stats_message, LOG_MESSAGE_SIZE, "STATS: %d Tasks not executed!", program_stats->total_tasks_not_executed);
    handle_log(stats_message);
    
    sem_post(mutex_stats);
}

void start_shutdown_routine(int signum) {
    handle_log("SIGNAL: Received SIGINT");

    handle_program_finish(signum);
}

void handle_program_finish(int signum) {
    int server_number, i;
    #ifdef DEBUG
    printf("handle_program_finish() <=> System manager\n");
    if (signum == SIGINT) {
        printf("Signalling SIGINT\n");
    }
    #endif

    handle_log("INFO: Starting shutdown routine, please wait");

    // * Signal other processes that they need to shutdown
    sem_wait(mutex_config);
    kill(program_configuration->task_manager_pid, SIGUSR1);
    kill(program_configuration->maintenance_manager_pid, SIGUSR1);
    kill(program_configuration->monitor_pid, SIGUSR1);
    sem_post(mutex_config);

    #ifdef DEBUG
    printf("Waiting for 3 processes to finish\n");
    #endif

    // * Wait for processes [ Maintenance Manager, Monitor, Task Manager ]
    wait(NULL);
    
    #ifdef DEBUG
    printf("Waiting for 2 processes to finish\n");
    #endif
    
    wait(NULL);
    
    #ifdef DEBUG
    printf("Waiting for 1 processes to finish\n");
    #endif
    
    wait(NULL);

    #ifdef DEBUG
    printf("All processes finished\n");
    #endif

    // TODO: Log not executed tasks (Log with the PID showing)

    display_stats(0);

    handle_log("INFO: Simulator Closing!");

    #ifdef DEBUG
    printf("Starting to cleanup resources!\n");
    #endif

    // ? Maybe use waitpid later ?
    // waitpid(program_configuration->monitor_pid, NULL, 0);
    // waitpid(program_configuration->task_manager_pid, NULL, 0);
    // waitpid(program_configuration->maintenance_manager_pid, NULL, 0);

    #ifdef DEBUG
    printf("\nSystem mngr PID =>%ld\nMonitor PID => %ld\nTask mngr PID => %ld\nMaintenance mngr PID => %ld\n\n", 
                                                (long)getpid(),
                                                (long)program_configuration->monitor_pid,
                                                (long)program_configuration->task_manager_pid,
                                                (long)program_configuration->maintenance_manager_pid);
    #endif

    // * Close / Shutdown Pthread
    sem_wait(mutex_config);

    pthread_mutex_destroy(&program_configuration->change_performance_mode_mutex);
    pthread_mutexattr_destroy(&program_configuration->mutex_attr);
    pthread_cond_destroy(&program_configuration->change_performance_mode);
    pthread_condattr_destroy(&program_configuration->cond_attr);

    server_number = program_configuration->edge_server_number;

    sem_post(mutex_config);

    sem_wait(mutex_servers);
    for (i = 0; i < server_number; i++) {
        pthread_mutex_destroy(&servers[i].edge_server_mutex);
        pthread_mutexattr_destroy(&servers[i].edge_mutex_attr);
        pthread_cond_destroy(&servers[i].edge_stopped);
        pthread_condattr_destroy(&servers[i].edge_cond_attr);
    }
    sem_post(mutex_servers);

    // * Unlink semaphores
    sem_unlink(MUTEX_LOGGER);
    sem_unlink(MUTEX_CONFIG);
    sem_unlink(MUTEX_SERVERS);
    sem_unlink(MUTEX_STATS);

    // * Close pipe
    close(fd_task_pipe);
    unlink(PIPE_NAME);

    // * Close Message Queue
    msgctl(message_queue_id, IPC_RMID, 0);

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

    // ? Shared memory
    /*
    * In struct prog_config, remove the field edge_server *servers
    * Instead, create shared memory with: 
    * size = sizeof(prog_config) + server_number * sizeof(edge_server) + {Insert aditional stuff needed}
    */
    
    #ifdef DEBUG
    printf("Creating shared memory!\n");
    #endif

    // Copied from factory_main.c PL4 Ex5
    if ((shmkey = ftok(".", getpid())) == (key_t) -1){
        perror("IPC error: ftok\n");
        exit(1);
    };
    // End of copied code

    shmid = shmget(shmkey, sizeof(prog_config) 
                        + sizeof(statistics)
                        + (sizeof(edge_server) * edge_server_number) 
                        , IPC_CREAT | 0777);
    if (shmid < 1) {
        perror("Error Creating shared memory\n");
        exit(1);
    }

    program_configuration = (prog_config *) shmat(shmid, NULL, 0);
   
    if (program_configuration < (prog_config*) 1) {
        perror("Error attaching memory\n");
        exit(1);
    }

    #ifdef DEBUG
    printf("Shared memory created!\n");
    #endif
    // Lock config SHM area
    sem_wait(mutex_config);

    program_configuration->queue_pos = queue_pos;
    program_configuration->max_wait = max_wait;
    program_configuration->edge_server_number = edge_server_number;
    program_configuration->servers_fully_booted = 0;
    //Performance modes = 1 <-> Default | 2  <-> High performance
    program_configuration->current_performance_mode = 1;

    pthread_condattr_init(&program_configuration->cond_attr);
    pthread_condattr_setpshared(&program_configuration->cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&program_configuration->change_performance_mode, &program_configuration->cond_attr);

    pthread_mutexattr_init(&program_configuration->mutex_attr);
    pthread_mutexattr_setpshared(&program_configuration->mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&program_configuration->change_performance_mode_mutex, &program_configuration->mutex_attr);

    // Unlock config memory area
    sem_post(mutex_config);

    program_stats = (statistics *) (program_configuration + 1);
    servers = (edge_server *) (program_stats + 1);

    // Lock servers SHM area
    sem_wait(mutex_servers);
    for (i = 0; i < program_configuration->edge_server_number; i++) {
        if (fgets(line, sizeof(line), qPtr) == NULL) {
            perror("Reached EOF\n");
        }
        // * Defaults
        servers[i].tasks_executed = 0;
        servers[i].is_shutting_down = 0;
        servers[i].maintenance_operation_performed = 0;
        servers[i].performance_mode = 1;

        pthread_condattr_init(&servers[i].edge_cond_attr);
        pthread_condattr_setpshared(&servers[i].edge_cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&servers[i].edge_stopped, &servers[i].edge_cond_attr);

        pthread_mutexattr_init(&servers[i].edge_mutex_attr);
        pthread_mutexattr_setpshared(&servers[i].edge_mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&servers[i].edge_server_mutex, &servers[i].edge_mutex_attr);

        
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

    // Lock Stats SHM area
    sem_wait(mutex_stats);

    // ? Initialize stats variables
    program_stats->avg_response_time = 0;
    program_stats->total_tasks_executed = 0;
    program_stats->total_tasks_not_executed = 0;

    sem_post(mutex_stats);
    // Unlock Stats SHM area


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

