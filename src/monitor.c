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
static int upper_task_trigger, lower_task_trigger, max_wait;
static int is_monitor_running = 1;
static pthread_t performance_mode_thread;
static pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shutdown_cond = PTHREAD_COND_INITIALIZER;

/*
? The Monitor's job is to control the edge servers, according to the established rules
? prog_config => current_performance_mode
*/

void monitor() {
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

    pthread_create(&performance_mode_thread, NULL, performance_checker, NULL);

    pthread_mutex_lock(&shutdown_mutex);
    pthread_cond_wait(&shutdown_cond, &shutdown_mutex);
    pthread_mutex_unlock(&shutdown_mutex);

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
    is_monitor_running = 0;

    pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
    pthread_cond_broadcast(&program_configuration->monitor_task_scheduled);
    pthread_cond_broadcast(&program_configuration->monitor_task_dispatched);
    pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);


    pthread_mutex_lock(&shutdown_mutex);
    pthread_cond_signal(&shutdown_cond);
    pthread_mutex_unlock(&shutdown_mutex);

    pthread_join(performance_mode_thread, NULL);
    
    exit(0);
}

void* performance_checker() {
    while (is_monitor_running) {
        if (monitor_performance_mode == 1) { // * Regular mode
            // ? Objective: check if task_queue->occupied_positions > upper_task_trigger
            // printf("[MONITOR] While Perf mode = 1\n");
            pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
            pthread_cond_wait(&program_configuration->monitor_task_scheduled, &program_configuration->monitor_variables_mutex);
            pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);

            // printf("\t\t\t[MONITOR] Woken at performance mode 1\n");
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
    }
    pthread_exit(NULL);
}
