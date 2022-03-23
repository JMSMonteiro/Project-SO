#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <wait.h>
#include "task_manager.h"
#include "system_manager.h"
#include "logger.h"
#include "edge_server.h"

void task_manager(prog_config *config) {
    int i;
    handle_log("INFO: Started Task Manager");

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
    // while (1) {

    // }
    exit(0);
}