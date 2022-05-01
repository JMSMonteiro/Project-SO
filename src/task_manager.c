// José Miguel Saraiva Monteiro - 2015235572

#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <wait.h>

#include "task_manager.h"
#include "system_manager.h"
#include "logger.h"
#include "edge_server.h"

#define EXIT_COMMAND "EXIT"
#define STATS_COMMAND "STATS"

pthread_t scheduler_id;
pthread_t dispatcher_id;
pthread_mutex_t task_manager_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t terminate_task_mngr_cond = PTHREAD_COND_INITIALIZER;
fd_set read_set;

int is_program_running = 1;
char pipe_string[LOG_MESSAGE_SIZE];
int *unnamed_pipes;
int server_number;

void task_manager() {
    int i;
    int char_number;
    pid_t system_manager_pid = getppid();
    handle_log("INFO: Task Manager Started");

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_task_mngr_shutdown);
    sem_wait(mutex_config);

    program_configuration->task_manager_pid = getpid();
    server_number = program_configuration->edge_server_number;

    sem_post(mutex_config);

    unnamed_pipes = (int*) malloc(sizeof(int) * (2 * server_number));

    // Create one unnamed pipe for each server process
    for (i = 0; i < server_number; i++) {
        pipe(&unnamed_pipes[i * 2]);
    }

    // Create Edge servers
    for (i = 0; i < server_number; i++) {
        // fork() == 0 => Child
        if (fork() == 0) {
            start_edge_server(&servers[i], i, server_number, unnamed_pipes);
        }
    }

    for (i = 0; i < server_number; i++) {
        close(unnamed_pipes[(i * 2)]);
    }
    
    pthread_create(&scheduler_id, NULL, scheduler, 0);
    pthread_create(&dispatcher_id, NULL, dispatcher, 0);

    #ifdef DEBUG
    printf("\t\tTask Manager PID = %d\n", getpid());

    printf("Edge server PIDs:\n");
    sem_wait(mutex_servers);
    for(int i = 0; i < server_number; i++) {
        printf("Server #%d PID = %d\n", i + 1, servers[i].server_pid);
    }
    sem_post(mutex_servers);
    #endif

    while (1) {
        FD_ZERO(&read_set);

        FD_SET(fd_task_pipe, &read_set);    // ? Set named pipe file descriptor

        if (select(fd_task_pipe + 1, &read_set, NULL, NULL, NULL) > 0) {

            if (FD_ISSET(fd_task_pipe, &read_set)) {
                char_number = read(fd_task_pipe, pipe_string, sizeof(pipe_string));
                pipe_string[char_number - 1] = '\0'; // ? Put a \0 at the string end

                if (strcmp(pipe_string, STATS_COMMAND) == 0) {
                    // * Signal System manager to print stats
                    kill(system_manager_pid, SIGTSTP);
                }
                else if (strcmp(pipe_string, EXIT_COMMAND) == 0) {
                    // * Signal System manager to shut down
                    kill(system_manager_pid, SIGINT);
                } 
            }
        }
    }
}

void handle_task_mngr_shutdown(int signum) {
    int i, server_pid;
    
    #ifdef DEBUG
    if (signum == SIGUSR1) {
        printf("Task Manager Received signal \"SIGUSR1\"\n");
    }
    else {
        printf("Task Manager Received signal \"UNKOWN\"\n");
    }
    printf("Shutting down task Manager <=> %d\n", getpid());
    #endif
    
    is_program_running = 0;

    for(i = 0; i < server_number; i++) {
        #ifdef DEBUG
        printf("Waiting for edge server %d\n", i + 1);
        #endif
        sem_wait(mutex_servers);
        server_pid = servers[i].server_pid;
        sem_post(mutex_servers);

        kill(server_pid, SIGUSR1);
        
        waitpid(server_pid, NULL, 0);
        #ifdef DEBUG
        printf("Edge server %d Finished\n", i + 1);
        #endif
    }

    #ifdef DEBUG
    printf("Servers finished, signalling cond var | Terminate Task MNGR\n");
    #endif
    
    pthread_mutex_lock(&task_manager_thread_mutex);
    pthread_cond_broadcast(&terminate_task_mngr_cond);
    pthread_mutex_unlock(&task_manager_thread_mutex);
    
    #ifdef DEBUG
    printf("TASK MANAGER => \tPRE \"Join Threads\"\n");
    #endif
    
    pthread_join(scheduler_id, NULL);
    pthread_join(dispatcher_id, NULL);

    #ifdef DEBUG
    printf("TASK MANAGER => \tPOST \"Join Threads\"\n");
    printf("Exiting from task Manager\n");
    #endif

    for(i = 0; i < server_number * 2; i++) {
        close(unnamed_pipes[i]);
    }

    free(unnamed_pipes);
    exit(0);    
}

void* scheduler(void *p) {
    handle_log("INFO: Scheduler Started");

    #ifdef DEBUG
    printf("I'm a thread! my PID => %lu\t[SCHEDULER]\n", scheduler_id);
    #endif

    pthread_mutex_lock(&task_manager_thread_mutex);
    while (is_program_running) {
        pthread_cond_wait(&terminate_task_mngr_cond, &task_manager_thread_mutex);
        #ifdef DEBUG
        printf("\t\tI'm IN THE CONDITION VARIABLE - SCHEDULER\n");
        #endif  
    }
    pthread_mutex_unlock(&task_manager_thread_mutex);
    #ifdef DEBUG
    printf("\t\tSCHEDULER - Unlocking mutex\n");
    #endif    

    #ifdef DEBUG
    printf("\t\tExiting thread: SCHEDULER\n");
    #endif

    pthread_exit(NULL);
}

void* dispatcher(void *p) {
    handle_log("INFO: Dispatcher Started");

    #ifdef DEBUG
    printf("I'm a thread! my PID => %lu\t[DISPATCHER]\n", dispatcher_id);
    #endif

    pthread_mutex_lock(&task_manager_thread_mutex);
    while (is_program_running) {
        pthread_cond_wait(&terminate_task_mngr_cond, &task_manager_thread_mutex);
        
        #ifdef DEBUG
        printf("\t\tI'm IN THE CONDITION VARIABLE - DISPATCHER \n");
        #endif
    }
    pthread_mutex_unlock(&task_manager_thread_mutex);
    #ifdef DEBUG
    printf("\t\tDISPATCHER - Unlocking mutex\n");
    #endif
    
    #ifdef DEBUG
    printf("\t\tExiting thread: DISPATCHER\n");
    #endif
    
    pthread_exit(NULL);
}
