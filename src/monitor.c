// José Miguel Saraiva Monteiro - 2015235572

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
