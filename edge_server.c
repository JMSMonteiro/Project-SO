#include "edge_server.h"
#include "logger.h"
#include "system_manager.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VCPU_NUMBER 2

pthread_t v_cpu[VCPU_NUMBER];

void start_edge_server(edge_server *server_config) {
    char log_message[64] = "INFO: Edge Server: '";
    int thread_id[VCPU_NUMBER] = { server_config->v_cpu1, server_config->v_cpu2};
    int i;
    strcat(log_message, server_config->name);
    strcat(log_message, "' Created");
    // ? Print => "Info: Edge server {Server_name} Created"
    handle_log(log_message); 

    // Create threads
    for (i = 0; i < VCPU_NUMBER; i++) {
        pthread_create(&v_cpu[i], NULL, edge_thread, &thread_id[i]);
    }

    // Wait for threads to finish
    for (i = 0; i < VCPU_NUMBER; i++) {
        pthread_join(v_cpu[i], NULL);
    }
}

void *edge_thread (void* p) {
    #ifdef DEBUG
    int w_id = *((int *) p);
    printf("I'm a thread | My power is %d!\n", w_id);
    #endif
    // TODO: Insert thread logic here



    pthread_exit(NULL);
}