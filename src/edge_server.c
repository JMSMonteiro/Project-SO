// José Miguel Saraiva Monteiro - 2015235572

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>

#include "edge_server.h"
#include "logger.h"
#include "system_manager.h"

#define VCPU_NUMBER 2
#define NANO_TO_SECOND 1000000000

typedef struct {
    int processing_power;
    int is_high_performance;
} thread_settings;

pthread_t v_cpu[VCPU_NUMBER];
pthread_t pipe_reader_thread;
pthread_mutex_t edge_server_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_for_task_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t end_of_maintenance_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t high_performance_mode_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t waiting_for_task = PTHREAD_COND_INITIALIZER;

static int server_is_running = 1;
static int server_needs_maintenance = 0;
static int performance_mode = 1;
static int server_index = -1;
static int stopped_threads = 0;
static pthread_t performance_checker;
static task_struct task_1 = {"", -1, -1, -1, -1};
static task_struct task_2 = {"", -1, -1, -1, -1};
static thread_settings t_settings[VCPU_NUMBER];

void start_edge_server(edge_server *server_config, int server_shm_position, int server_number, int *unnamed_pipes) {
    maintenance_message message_rcvd;
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
    server_config->available_for_tasks = performance_mode == 2 ? 2 : 1;
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
    for (i = 0; i < server_index; i++) {
        if (i != server_index) {
            close(unnamed_pipes[(i * 2)]);       // 0 = Read
            close(unnamed_pipes[(i * 2) + 1]);   // 1 = Write
        }
        close(unnamed_pipes[(i * 2) + 1]);   // 1 = Write
    }

    // Create threads
    t_settings[0].processing_power = vcpu_power[0];
    t_settings[0].is_high_performance = vcpu_power[0] > vcpu_power[1] ? 1 : 0;
    pthread_create(&v_cpu[0], NULL, edge_thread, &t_settings[0]);
    
    t_settings[1].processing_power = vcpu_power[1];
    t_settings[1].is_high_performance = vcpu_power[1] > vcpu_power[0] ? 1 : 0;
    pthread_create(&v_cpu[1], NULL, edge_thread, &t_settings[1]);

    pthread_create(&performance_checker, NULL, performance_mode_checker, NULL);
    pthread_create(&pipe_reader_thread, NULL, pipe_reader, unnamed_pipes);

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
            sem_wait(mutex_servers);
            servers[server_index].performance_mode = 0; // Stopped
            servers[server_index].available_for_tasks = 0; 
            servers[server_index].maintenance_time = message_rcvd.maintenance_time; 
            sem_post(mutex_servers);

            pthread_mutex_lock(&edge_server_thread_mutex);
            server_needs_maintenance = 1;
            pthread_mutex_unlock(&edge_server_thread_mutex);
            
            pthread_mutex_lock(&waiting_for_task_mutex);
            pthread_cond_broadcast(&waiting_for_task);
            pthread_mutex_unlock(&waiting_for_task_mutex);
            
            #ifdef DEBUG
            printf("\t[SERVER %d] Received message to STOP!\n", server_index);
            #endif
        }
        // * If Server is going to resume work
        else {
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                                "INFO: Server #%d Maintained, going back to work",
                                server_index + 1);
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

    pthread_mutex_lock(&waiting_for_task_mutex);
    pthread_cond_broadcast(&waiting_for_task);
    pthread_mutex_unlock(&waiting_for_task_mutex);
    
    for (i = 0; i < VCPU_NUMBER; i++) {
        pthread_join(v_cpu[i], NULL);
    }

    pthread_join(performance_checker, NULL);
    // Request thread cancelation of thread "pipe_reader" (otherwise it'll never shut down)
    pthread_cancel(pipe_reader_thread);
    pthread_join(pipe_reader_thread, NULL);

    pthread_mutex_lock(&servers[server_index].edge_server_mutex);
    pthread_cond_broadcast(&servers[server_index].edge_stopped);
    pthread_mutex_unlock(&servers[server_index].edge_server_mutex);

    pthread_mutex_destroy(&edge_server_thread_mutex);
    pthread_mutex_destroy(&waiting_for_task_mutex);

    pthread_cond_destroy(&end_of_maintenance_cond);
    pthread_cond_destroy(&high_performance_mode_cond);
    pthread_cond_destroy(&waiting_for_task);

    #ifdef DEBUG
    printf("Exiting from Edge Server <=> %d\n", getpid());
    #endif

    exit(0);    
}

void *pipe_reader(void* p) {
    int *unnamed_pipes = ((int *) p);
    int accepted_task = 0;
    char *pipe_task_message = NULL;
    char pipe_string[LOG_MESSAGE_SIZE / 2];
    char log_message[LOG_MESSAGE_SIZE];
    time_t current_time;
    task_struct task_received;

    #ifdef DEBUG
    printf("[PIPE READER] UP AND RUNNING\n");
    #endif

    while(server_is_running) {
        accepted_task = 0;

        read(unnamed_pipes[server_index * 2], &pipe_string, sizeof(pipe_string));
        // printf("\t[SERVER] Server #%d received task => %s\n", server_index + 1, pipe_string);

        // Process task into task_struct
        pipe_task_message = strtok(pipe_string, ":");
        strcpy(task_received.task_id, pipe_task_message);

        pipe_task_message = strtok(NULL, ":");
        task_received.mips = atoi(pipe_task_message);
        
        pipe_task_message = strtok(NULL, ":");
        task_received.exec_time = atoi(pipe_task_message);

        pipe_task_message = strtok(NULL, ":");
        task_received.arrived_at = atol(pipe_task_message);

        pipe_task_message = strtok(NULL, ":");
        current_time = atol(pipe_task_message);

        task_received.priority = 1;

        sem_wait(mutex_servers);
        pthread_mutex_lock(&edge_server_thread_mutex);
        // printf("[TESTING] \'%s\' \t%lu + %d + %d <= %lu = %d | task_1.priority = %d\n",task_received.task_id, current_time, task_received.mips / t_settings[0].processing_power, task_received.mips % t_settings[0].processing_power ? 1 : 0, task_received.arrived_at + task_received.exec_time, ((current_time + (task_received.mips / t_settings[0].processing_power 
        //                       + (task_received.mips % t_settings[0].processing_power ? 1 : 0))) 
        //                       <= task_received.arrived_at + task_received.exec_time), task_1.priority);
        if (task_1.priority < 0 
                        && ((current_time + (task_received.mips / t_settings[0].processing_power 
                            + (task_received.mips % t_settings[0].processing_power ? 1 : 0))) 
                            <= task_received.arrived_at + task_received.exec_time)) {
            // if vcpu1 is free

            #ifdef DEBUG
            printf("[TESTING] \'%s\' \t%lu + %d + %d > %lu <= %d\n",task_received.task_id, current_time, task_received.mips / t_settings[0].processing_power, task_received.mips % t_settings[0].processing_power ? 1 : 0, task_received.arrived_at + task_received.exec_time, ((current_time + (task_received.mips / t_settings[0].processing_power 
                              + (task_received.mips % t_settings[0].processing_power ? 1 : 0))) 
                                <= task_received.arrived_at + task_received.exec_time));
            #endif

            task_1 = task_received;
            accepted_task = 1;
        }
        if (!accepted_task && task_2.priority < 0 && performance_mode == 2 
                        && ((current_time + (task_received.mips / t_settings[1].processing_power 
                            + (task_received.mips % t_settings[1].processing_power ? 1 : 0))) 
                            <= task_received.arrived_at + task_received.exec_time)) {
            // if vcp1 is occupied and vcpu2 is active
            task_2 = task_received;
            accepted_task = 1;
        }
        pthread_mutex_unlock(&edge_server_thread_mutex);
        sem_post(mutex_servers);

        if (accepted_task) {
            pthread_mutex_lock(&waiting_for_task_mutex);
            pthread_cond_broadcast(&waiting_for_task);
            pthread_mutex_unlock(&waiting_for_task_mutex);
        }
        else {
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                    "WARNING: task \'%s\' removed - Can\'t be done on time", 
                    task_received.task_id);
            handle_log(log_message);
            sem_wait(mutex_stats);
            program_stats->total_tasks_not_executed++;
            sem_post(mutex_stats);
        }
    }
    
    pthread_exit(NULL);
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
        if (!server_needs_maintenance) {
            sem_wait(mutex_servers);
            servers[server_index].performance_mode = performance_mode;
            sem_post(mutex_servers);
        }
        pthread_cond_broadcast(&high_performance_mode_cond);
        pthread_mutex_unlock(&edge_server_thread_mutex);
    }
    pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

    pthread_exit(NULL);
}

void *edge_thread (void* p) {
    thread_settings settings = *((thread_settings *) p);
    char log_message[LOG_MESSAGE_SIZE];
    int avoid_duplicated_logs = 0;

    while (server_is_running) {
        
        if (server_needs_maintenance) { 
            sem_wait(mutex_servers);
            servers[server_index].available_for_tasks--;
            sem_post(mutex_servers);
            
            pthread_mutex_lock(&edge_server_thread_mutex);
            stopped_threads++;
            if (stopped_threads == 2) { // Signal MM that this server is stopped
                if (avoid_duplicated_logs == 0) {
                    avoid_duplicated_logs++;
                    sem_wait(mutex_servers);
                    snprintf(log_message, LOG_MESSAGE_SIZE, 
                                    "INFO: Server #%d Stopped, going to maintenance for (%ds)",
                                    server_index + 1,
                                    servers[server_index].maintenance_time);
                    handle_log(log_message);
                    sem_post(mutex_servers);
                }
                pthread_mutex_lock(&servers[server_index].edge_server_mutex);
                pthread_cond_broadcast(&servers[server_index].edge_stopped);
                pthread_mutex_unlock(&servers[server_index].edge_server_mutex);
            }

            pthread_cond_wait(&end_of_maintenance_cond, &edge_server_thread_mutex);

            server_needs_maintenance = 0;
            
            pthread_mutex_unlock(&edge_server_thread_mutex);

            if (!settings.is_high_performance) {
                sem_wait(mutex_servers);
                servers[server_index].performance_mode = performance_mode;
                servers[server_index].available_for_tasks = performance_mode;
                sem_post(mutex_servers);
                
                avoid_duplicated_logs = 0;

                pthread_mutex_lock(&edge_server_thread_mutex);
                pthread_cond_broadcast(&high_performance_mode_cond);
                pthread_mutex_unlock(&edge_server_thread_mutex);
            }

            pthread_mutex_lock(&edge_server_thread_mutex);
            stopped_threads--;
            pthread_mutex_unlock(&edge_server_thread_mutex);
        }
        // * Only if it's the performance thread
        else if (settings.is_high_performance == 1 && performance_mode != 2) {
            pthread_mutex_lock(&edge_server_thread_mutex);
            if (!server_needs_maintenance) {
                sem_wait(mutex_servers);
                servers[server_index].available_for_tasks = performance_mode;
                sem_post(mutex_servers);
            }

            stopped_threads++;
            pthread_cond_wait(&high_performance_mode_cond, &edge_server_thread_mutex);
            stopped_threads--;

            pthread_mutex_unlock(&edge_server_thread_mutex);
        }
        // * Does regular work
        else {
            pthread_mutex_lock(&program_configuration->server_available_for_task_mutex);
            pthread_cond_broadcast(&program_configuration->server_available_for_task);
            pthread_mutex_unlock(&program_configuration->server_available_for_task_mutex);

            pthread_mutex_lock(&waiting_for_task_mutex);
            pthread_cond_wait(&waiting_for_task, &waiting_for_task_mutex);
            pthread_mutex_unlock(&waiting_for_task_mutex);

            if ((task_1.priority > 0 && !settings.is_high_performance)      // if it's vcpu1 and has task
                || (task_2.priority > 0 && settings.is_high_performance)) { // if it's vcpu2 and has task
                process_task(settings);
            }
        }
    }

    #ifdef DEBUG
    printf("\tExiting from vcpu thread!\n");
    #endif

    pthread_exit(NULL);
}

void process_task(thread_settings settings) {
    struct timespec remaining, request = {0, 0};
    char log_message[LOG_MESSAGE_SIZE];
    float time_in_float;
    int time_in_int;
    pthread_mutex_lock(&edge_server_thread_mutex);
    if (!settings.is_high_performance) {
        // vcpu1
        time_in_int = task_1.mips / settings.processing_power;
        request.tv_sec = time_in_int;
        time_in_float = (float) task_1.mips / settings.processing_power;

        request.tv_nsec = (time_in_float - time_in_int) * NANO_TO_SECOND;
    }
    else {
        time_in_int = task_2.mips / settings.processing_power;
        request.tv_sec = time_in_int;
        time_in_float = (float) task_2.mips / settings.processing_power;

        request.tv_nsec = (time_in_float - time_in_int) * NANO_TO_SECOND;
    }
    pthread_mutex_unlock(&edge_server_thread_mutex);
    nanosleep(&request, &remaining);

    snprintf(log_message, LOG_MESSAGE_SIZE, "SERVER: Server #%d - vCPU #%d - Finished task: \'%s\'", server_index + 1, settings.is_high_performance ? 2 : 1, settings.is_high_performance ? task_2.task_id : task_1.task_id);
    handle_log(log_message);

    sem_wait(mutex_stats);
    program_stats->total_tasks_executed++;
    sem_post(mutex_stats);

    sem_wait(mutex_servers);
    servers[server_index].tasks_executed++;

    pthread_mutex_lock(&edge_server_thread_mutex);
    if (!settings.is_high_performance) {
        task_1.priority = -1;
    } else {
        task_2.priority = -1;
    }

    if (!server_needs_maintenance) {
        servers[server_index].available_for_tasks++;
    }
    pthread_mutex_unlock(&edge_server_thread_mutex);
    sem_post(mutex_servers);

    return;
}
