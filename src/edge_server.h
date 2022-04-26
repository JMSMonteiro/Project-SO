// José Miguel Saraiva Monteiro - 2015235572

#ifndef _edge_server_h__
#define _edge_server_h__

#include "system_manager.h"
#include "task_manager.h"

void start_edge_server(edge_server *server_config, int server_shm_position);
void handle_edge_shutdown(int signum);
void *edge_thread (void* p);

#endif
