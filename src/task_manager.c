// José Miguel Saraiva Monteiro - 2015235572

#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
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

pthread_t scheduler_id;
pthread_t dispatcher_id;
pthread_mutex_t task_manager_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t terminate_task_mngr_cond = PTHREAD_COND_INITIALIZER;

int is_program_running = 1;

void task_manager() {
    int i;
    handle_log("INFO: Task Manager Started");

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_task_mngr_shutdown);
    sem_wait(mutex_config);

    program_configuration->task_manager_pid = getpid();

    sem_post(mutex_config);

    // Create Edge servers
    for (i = 0; i < program_configuration->edge_server_number; i++) {
        // fork() == 0 => Child
        if (fork() == 0) {
            start_edge_server(&servers[i], i);
        }
    }
    
    pthread_create(&scheduler_id, NULL, scheduler, 0);
    pthread_create(&dispatcher_id, NULL, dispatcher, 0);

    #ifdef DEBUG
    printf("\t\tTask Manager PID = %d\n", getpid());

    printf("Edge server PIDs:\n");
    sem_wait(mutex_servers);
    for(int i = 0; i < program_configuration->edge_server_number; i++) {
        printf("Server #%d PID = %d\n", i + 1, servers[i].server_pid);
    }
    sem_post(mutex_servers);
    #endif

    while (1) {

    }
}

void handle_task_mngr_shutdown(int signum) {
    int i, server_pid, server_number;
    
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

    sem_wait(mutex_config);
    server_number = program_configuration->edge_server_number;
    sem_post(mutex_config);

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
