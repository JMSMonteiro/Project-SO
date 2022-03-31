// José Miguel Saraiva Monteiro - 2015235572

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <wait.h>

#include "task_manager.h"
#include "system_manager.h"
#include "logger.h"
#include "edge_server.h"

pthread_t scheduler_id;

void task_manager(prog_config *config) {
    int i;
    handle_log("INFO: Task Manager Started");

    //Create 
    for (i = 0; i < config->edge_server_number; i++) {
        // fork() == 0 => Child
        if (fork() == 0) {
            start_edge_server(&config->servers[i]);
        }
        else {
            wait(NULL);
            exit(0);
        }
    }
    
    pthread_create(&scheduler_id, NULL, scheduler, 0);

    pthread_join(scheduler_id, NULL);
    // while (1) {

    // }
    exit(0);
}

void* scheduler(void *p) {
    handle_log("INFO: Scheduler Started");
    pthread_exit(NULL);
}
