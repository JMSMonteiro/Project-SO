#ifndef edge_server_h__
#define edge_server_h__

#include "system_manager.h"

void start_edge_server(edge_server *server_config);
void *edge_thread (void* p);

#endif