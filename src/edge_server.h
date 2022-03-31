// José Miguel Saraiva Monteiro - 2015235572

#ifndef _edge_server_h__
#define _edge_server_h__

#include "system_manager.h"

void start_edge_server(edge_server *server_config);
void *edge_thread (void* p);

#endif
