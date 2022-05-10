// José Miguel Saraiva Monteiro - 2015235572

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "logger.h"
#include "monitor.h"
#include "system_manager.h"

#define UPPER_TRIGGER_PERCENTAGE 80
#define LOWER_TRIGGER_PERCENTAGE 20

static int monitor_performance_mode;

/*
? The Monitor's job is to control the edge servers, according to the established rules
? prog_config => current_performance_mode
*/

void monitor() {
    int upper_task_trigger, lower_task_trigger, max_wait;
    handle_log("INFO: Monitor Started");

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_monitor_shutdown);

    sem_wait(mutex_config);

    program_configuration->monitor_pid = getpid();

    monitor_performance_mode = program_configuration->current_performance_mode;
    upper_task_trigger = (program_configuration->queue_pos * UPPER_TRIGGER_PERCENTAGE) / 100;
    lower_task_trigger = (program_configuration->queue_pos * LOWER_TRIGGER_PERCENTAGE) / 100;
    max_wait = program_configuration->max_wait;

    sem_post(mutex_config);

    while (1) {
        sem_wait(mutex_config);
        if (program_configuration->simulator_shutting_down) {
            // printf("[MONITOR] Leaving while loop\n");
            sem_post(mutex_config);
            exit(0);
        }
        sem_post(mutex_config);
        if (monitor_performance_mode == 1) { // * Regular mode
            // ? Objective: check if task_queue->occupied_positions > upper_task_trigger
            // printf("[MONITOR] While Perf mode = 1\n");
            pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
            pthread_cond_wait(&program_configuration->monitor_task_scheduled, &program_configuration->monitor_variables_mutex);
            pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);

            // printf("[MONITOR] Woken at performance mode 1\n");
            
            sem_wait(mutex_tasks);
            if (task_queue->occupied_positions > upper_task_trigger && task_queue->time_to_process_task > max_wait) {
                handle_log("MONITOR: Swapping servers to high performance mode");
                swap_performance_mode();
            }
            sem_post(mutex_tasks);
        }
        else if (monitor_performance_mode == 2) { // * Performance mode
            // ? Objective: check if task_queue->occupied_positions < lower_task_trigger
            pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
            pthread_cond_wait(&program_configuration->monitor_task_dispatched, &program_configuration->monitor_variables_mutex);
            pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);
            sem_wait(mutex_tasks);
            if (task_queue->occupied_positions < lower_task_trigger) {
                handle_log("MONITOR: Swapping servers to regular performance mode");
                swap_performance_mode();
            }
            sem_post(mutex_tasks);

        } 
        // // ! IMPORTANT
        // // TODO Remove Sleep, use a thread to verify SHM status and do the changes
        // sleep(5);

        // sem_wait(mutex_config);
        // program_configuration->current_performance_mode = 2;
        // sem_post(mutex_config);

        // #ifdef DEBUG
        // printf("[MONITOR] => HIGH PERFORMANCE ACTIVATE! (%d)\n", program_configuration->current_performance_mode);
        // #endif

        // pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
        // pthread_cond_broadcast(&program_configuration->change_performance_mode);
        // pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

        // // ! IMPORTANT
        // // TODO Remove Sleep, use a thread to verify SHM status and do the changes
        // sleep(5);

        // sem_wait(mutex_config);
        // program_configuration->current_performance_mode = 1;
        // sem_post(mutex_config);

        // #ifdef DEBUG
        // printf("[MONITOR] => Regular mode!\n");
        // #endif

        // pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
        // pthread_cond_broadcast(&program_configuration->change_performance_mode);
        // pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);
    }
}

void swap_performance_mode() {
    sem_wait(mutex_config);
    program_configuration->current_performance_mode = monitor_performance_mode == 1 ? 2 : 1;
    monitor_performance_mode = program_configuration->current_performance_mode;
    sem_post(mutex_config);

    // * Broadcast that the performance mode has been changed
    pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
    pthread_cond_broadcast(&program_configuration->change_performance_mode);
    pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);
}

void handle_monitor_shutdown(int signum) {
    #ifdef DEBUG
    if (signum == SIGUSR1) {
        printf("Monitor received signal \"SIGUSR1\"\n");
    }
    printf("Shutting down Monitor\n");
    #endif



    exit(0);
}
