// José Miguel Saraiva Monteiro - 2015235572

#ifndef _system_manager_h__
#define _system_manager_h__

#include <stdlib.h>

// #define DEBUG   // Remove/Comment this line to remove debug messages

typedef struct {
    char name[64];
    int v_cpu1;
    int v_cpu2;
} edge_server;

typedef struct {
    int queue_pos;
    int max_wait;
    int edge_server_number;
    edge_server *servers;
} prog_config;

void load_config(char *file_name);

#endif