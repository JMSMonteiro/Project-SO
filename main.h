#ifndef main_h__
#define main_h__

#include <stdlib.h>

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

void load_config();

#endif