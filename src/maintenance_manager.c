// José Miguel Saraiva Monteiro - 2015235572

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "logger.h"
#include "maintenance_manager.h"
#include "system_manager.h"

/*
? Generates maintenance messages, get responses and manage maintnance
*/

void maintenance_manager() {
    handle_log("INFO: Maintenance Manager Started");

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_maintenance_mngr_shutdown);


    sem_wait(mutex_config);

    program_configuration->maintenance_monitor_pid = getpid();

    sem_post(mutex_config);

    while(1) {
        // * Maintenance Interval || Maintenance time => Random [1 - 5] seconds
        // ? mngr => Server -> Going to maintenance ops
        // ? Server => mngr -> Ready for intervention
        // ? Server = Stopped
        // ? mngr => Server -> Restart working


    }    
    exit(0);
}

void handle_maintenance_mngr_shutdown(int signum) {
    #ifdef DEBUG
    if (signum == SIGUSR1) {
        printf("Maintenance Manager received signal \"SIGUSR1\"\n");
    }
    printf("Shutting down Maintenance Manager\n");
    #endif

    exit(0);
}
