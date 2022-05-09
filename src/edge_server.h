// José Miguel Saraiva Monteiro - 2015235572

#ifndef _edge_server_h__
#define _edge_server_h__

#include "system_manager.h"
#include "task_manager.h"

void start_edge_server(edge_server *server_config, int server_shm_position, int server_number, int* unnamed_pipe);
void handle_edge_shutdown(int signum);
void *pipe_reader(void* p);
void *performance_mode_checker ();
void *edge_thread (void* p);
void process_task();

#endif
