// José Miguel Saraiva Monteiro - 2015235572

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "logger.h"
#include "monitor.h"
#include "system_manager.h"

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

    sem_post(mutex_config);

    while (1) {
        // ! IMPORTANT
        // TODO Remove Sleep, use a thread to verify SHM status and do the changes
        sleep(5);

        sem_wait(mutex_config);
        program_configuration->current_performance_mode = 2;
        sem_post(mutex_config);

        #ifdef DEBUG
        printf("[MONITOR] => HIGH PERFORMANCE ACTIVATE! (%d)\n", program_configuration->current_performance_mode);
        #endif

        pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
        pthread_cond_broadcast(&program_configuration->change_performance_mode);
        pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

        // ! IMPORTANT
        // TODO Remove Sleep, use a thread to verify SHM status and do the changes
        sleep(5);

        sem_wait(mutex_config);
        program_configuration->current_performance_mode = 1;
        sem_post(mutex_config);

        #ifdef DEBUG
        printf("[MONITOR] => Regular mode!\n");
        #endif

        pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
        pthread_cond_broadcast(&program_configuration->change_performance_mode);
        pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);
    }
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
