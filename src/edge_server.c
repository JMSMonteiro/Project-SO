// José Miguel Saraiva Monteiro - 2015235572

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

#include "edge_server.h"
#include "logger.h"
#include "system_manager.h"

#define VCPU_NUMBER 2

pthread_t v_cpu[VCPU_NUMBER];
pthread_mutex_t edge_server_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t terminate_server_cond = PTHREAD_COND_INITIALIZER;

static int server_is_running = 1;
static int server_index = -1;

void start_edge_server(edge_server *server_config, int server_shm_position, int server_number, int* unnamed_pipe) {
    maintenance_message message_rcvd;
    char log_message[LOG_MESSAGE_SIZE];
    int thread_id[VCPU_NUMBER] = { server_config->v_cpu1, server_config->v_cpu2};
    int i;

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_edge_shutdown);

    // Save position in SHM "array" for later use
    server_index = server_shm_position;

    sem_wait(mutex_servers);
    server_config->server_pid = getpid();
    sem_post(mutex_servers);

    sem_wait(mutex_servers);
    snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Edge Server: \'%s\' Created",
                                server_config->name);
    sem_post(mutex_servers);

    // ? Handle Pipe
    // ? The end of the pipe in the server is meant to be read from
    // ? Task mngr => Edge server (Tasks) 
    // ? Only this server's specific end of a pipe meant to be read from is needed 
    for (i = 0; i < server_number; i++) {
        if (i == server_index) {
            close(unnamed_pipe[(i * 2) + 1]);   // 1 = Write

        }
        close(unnamed_pipe[(i * 2)]);       // 0 = Read
        close(unnamed_pipe[(i * 2) + 1]);   // 1 = Write
    }
    
    // ? Print => "Info: Edge server {Server_name} Created"
    handle_log(log_message); 

    #ifdef DEBUG
    sem_wait(mutex_servers);
    printf("Edge Server %s with PID %d\n", server_config->name, server_config->server_pid);
    sem_post(mutex_servers);
    #endif

    // Create threads
    for (i = 0; i < VCPU_NUMBER; i++) {
        #ifdef DEBUG
        sem_wait(mutex_servers);
        printf("\tServer %s Making thread #%d\n", server_config->name, i + 1);
        sem_post(mutex_servers);
        #endif
    
        pthread_create(&v_cpu[i], NULL, edge_thread, &thread_id[i]);
    }

    while (1) {
        msgrcv(message_queue_id, &message_rcvd, sizeof(maintenance_message) - sizeof(long), (server_index + 1), 0);
        if (message_rcvd.stop_flag) {
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Server %d Stopped, going to maintenance for (%ds)",
                                server_index,
                                message_rcvd.maintenance_time);
            handle_log(log_message);
            #ifdef DEBUG
            printf("\t[SERVER %d] Received message to STOP!\n", server_index);
            #endif
        }
        else {
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Server %d Maintained, going back to work",
                                server_index);
            handle_log(log_message);
            #ifdef DEBUG
            printf("\t[SERVER %d] Received message to RESUME\n", server_index);
            #endif
        }
    }
}

void handle_edge_shutdown(int signum) {
    int i;
    #ifdef DEBUG
    printf("Shutting down Edge Server <=> %d\n", getpid());
    #endif

    server_is_running = 0;

    pthread_mutex_lock(&edge_server_thread_mutex);
    pthread_cond_broadcast(&terminate_server_cond);
    pthread_mutex_unlock(&edge_server_thread_mutex);

    for (i = 0; i < VCPU_NUMBER; i++) {
        pthread_join(v_cpu[i], NULL);
    }

    pthread_mutex_destroy(&edge_server_thread_mutex);

    #ifdef DEBUG
    printf("Exiting from Edge Server <=> %d\n", getpid());
    #endif

    exit(0);    
}

void *edge_thread (void* p) {
    #ifdef DEBUG
    int w_id = *((int *) p);
    printf("I'm a thread | My power is %d!\n", w_id);
    #endif

    pthread_mutex_lock(&edge_server_thread_mutex);
    while (server_is_running) {
        pthread_cond_wait(&terminate_server_cond, &edge_server_thread_mutex);
    }
    pthread_mutex_unlock(&edge_server_thread_mutex);

    // TODO: Insert thread logic here

    pthread_exit(NULL);
}
