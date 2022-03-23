#include "logger.h"
#include "system_manager.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void start_edge_server(edge_server *server_config) {
    char log_message[64] = "INFO: Edge Server: '";
    strcat(log_message, server_config->name);
    strcat(log_message, "' Created");
    // ? Print => "Info: Edge server {Server_name} Created"
    handle_log(log_message); 
}