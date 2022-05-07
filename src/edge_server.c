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

typedef struct {
    int processing_power;
    int is_high_performance;
} thread_settings;

pthread_t v_cpu[VCPU_NUMBER];
pthread_t performance_checker;
pthread_mutex_t edge_server_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t end_of_maintenance_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t high_performance_mode_cond = PTHREAD_COND_INITIALIZER;

static int server_is_running = 1;
static int server_needs_maintenance = 0;
static int performance_mode = 1;
static int server_index = -1;

void start_edge_server(edge_server *server_config, int server_shm_position, int server_number, int* unnamed_pipe) {
    maintenance_message message_rcvd;
    thread_settings t_settings[VCPU_NUMBER];
    char log_message[LOG_MESSAGE_SIZE];
    int vcpu_power[VCPU_NUMBER] = { server_config->v_cpu1, server_config->v_cpu2};
    int i;

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_edge_shutdown);

    // Save position in SHM "array" for later use
    server_index = server_shm_position;

    sem_wait(mutex_servers);
    server_config->server_pid = getpid();
    performance_mode = server_config->performance_mode;
    snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Edge Server: \'%s\' Created",
                                server_config->name);
    sem_post(mutex_servers);

    // ? Print => "Info: Edge server {Server_name} Created"
    handle_log(log_message); 
    
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

    #ifdef DEBUG
    sem_wait(mutex_servers);
    printf("Edge Server %s with PID %d\n", server_config->name, server_config->server_pid);
    sem_post(mutex_servers);
    #endif

    // Create threads
    #ifdef DEBUG
    sem_wait(mutex_servers);
    printf("\tServer %s Making thread #%d\n", server_config->name, 1);
    sem_post(mutex_servers);
    #endif
    t_settings[0].processing_power = vcpu_power[0];
    t_settings[0].is_high_performance = vcpu_power[0] > vcpu_power[1] ? 1 : 0;
    pthread_create(&v_cpu[0], NULL, edge_thread, &t_settings[0]);
    
    #ifdef DEBUG
    sem_wait(mutex_servers);
    printf("\tServer %s Making thread #%d\n", server_config->name, 2);
    sem_post(mutex_servers);
    #endif
    t_settings[1].processing_power = vcpu_power[1];
    t_settings[1].is_high_performance = vcpu_power[1] > vcpu_power[0] ? 1 : 0;
    pthread_create(&v_cpu[1], NULL, edge_thread, &t_settings[1]);

    pthread_create(&performance_checker, NULL, performance_mode_checker, NULL);

    sem_wait(mutex_config);

    program_configuration->servers_fully_booted++;

    sem_post(mutex_config);

    pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
    pthread_cond_broadcast(&program_configuration->change_performance_mode);
    pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

    while (1) {
        msgrcv(message_queue_id, &message_rcvd, sizeof(maintenance_message) - sizeof(long), (server_index + 1), 0);
        // * If server is going to maintenance
        if (message_rcvd.stop_flag) {
            server_needs_maintenance = 1;
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Server %d Stopped, going to maintenance for (%ds)",
                                server_index,
                                message_rcvd.maintenance_time);
            handle_log(log_message);
            #ifdef DEBUG
            printf("\t[SERVER %d] Received message to STOP!\n", server_index);
            #endif
        }
        // * If Server is going to resume work
        else {
            server_needs_maintenance = 0;
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Server %d Maintained, going back to work",
                                server_index);
            handle_log(log_message);

            sem_wait(mutex_servers);
            servers[server_shm_position].maintenance_operation_performed++;
            sem_post(mutex_servers);
            
            pthread_mutex_lock(&edge_server_thread_mutex);
            // * Signal thread(s) to resume work, maintenance is done
            pthread_cond_broadcast(&end_of_maintenance_cond);
            pthread_mutex_unlock(&edge_server_thread_mutex);
            #ifdef DEBUG
            printf("\t[SERVER %d] Received message to RESUME\n", server_index);
            #endif
        }
    }
}

void handle_edge_shutdown(int signum) {
    int i;
    #ifdef DEBUG
    printf("\t\t\tShutting down Edge Server <=> %d\n", getpid());
    #endif

    server_is_running = 0;

    sem_wait(mutex_servers);
    servers[server_index].is_shutting_down = 1;
    sem_post(mutex_servers);

    // Signal everything to make sure threads  terminate and don't get stuck
    pthread_mutex_lock(&edge_server_thread_mutex);
    pthread_cond_broadcast(&high_performance_mode_cond);
    pthread_cond_broadcast(&end_of_maintenance_cond);
    pthread_mutex_unlock(&edge_server_thread_mutex);

    pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
    pthread_cond_broadcast(&program_configuration->change_performance_mode);
    pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

    for (i = 0; i < VCPU_NUMBER; i++) {
        pthread_join(v_cpu[i], NULL);
    }

    pthread_join(performance_checker, NULL);

    pthread_mutex_destroy(&edge_server_thread_mutex);

    #ifdef DEBUG
    printf("Exiting from Edge Server <=> %d\n", getpid());
    #endif

    exit(0);    
}

void *performance_mode_checker () {
    // * Thread used to check if the server's performance mode needs to change or not
    pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
    while (server_is_running) {
        // * Cond Var -> wait for signal to change performance mode
        pthread_cond_wait(&program_configuration->change_performance_mode, &program_configuration->change_performance_mode_mutex);
        // * Controlled access to shm => get current performance mode
        sem_wait(mutex_config);
        performance_mode = program_configuration->current_performance_mode;
        sem_post(mutex_config);
        
        pthread_mutex_lock(&edge_server_thread_mutex);
        pthread_cond_broadcast(&high_performance_mode_cond);
        pthread_mutex_unlock(&edge_server_thread_mutex);
    }
    pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

    pthread_exit(NULL);
}

void *edge_thread (void* p) {
    thread_settings settings = *((thread_settings *) p);
    struct timespec remaining, request = {1, 0};
    #ifdef DEBUG
    printf("I'm a vCPU thread | [POWER] = %d! [Performance] = %d\n", settings.processing_power, settings.is_high_performance);
    if (settings.is_high_performance) {
        printf("\t\tI\'m the high performance thread!\n");
    }
    #endif

    while (server_is_running) {
        // * Server going into maintenance
        if (server_needs_maintenance) { 
            #ifdef DEBUG
            printf("\t[THREAD] with [POWER] = %d [STOPPING]!\n", settings.processing_power);
            #endif
            pthread_mutex_lock(&edge_server_thread_mutex);
            pthread_cond_wait(&end_of_maintenance_cond, &edge_server_thread_mutex);
            pthread_mutex_unlock(&edge_server_thread_mutex);
            #ifdef DEBUG
            printf("\t\t[THREAD] with [POWER] = %d [RESUMING]!\n", settings.processing_power);
            #endif
        }
        // * Only if it's the performance thread
        else if (settings.is_high_performance == 1 && performance_mode != 2) {
            pthread_mutex_lock(&edge_server_thread_mutex);
            pthread_cond_wait(&high_performance_mode_cond, &edge_server_thread_mutex);
            pthread_mutex_unlock(&edge_server_thread_mutex);
        }
        // * Does regular work
        else {
            #ifdef DEBUG
            printf("\tThread with %d POWER working\n", settings.processing_power);
            #endif
            nanosleep(&request, &remaining);
        }
    }

    // TODO: Insert thread logic here

    #ifdef DEBUG
    printf("\tExiting from vcpu thread!\n");
    #endif

    pthread_exit(NULL);
}
