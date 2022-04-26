// José Miguel Saraiva Monteiro - 2015235572

#ifndef _task_manager_h__
#define _task_manager_h__

#include "system_manager.h"

void task_manager();
void handle_task_mngr_shutdown(int signum);
void *scheduler(void* p);
void *dispatcher(void* p);

#endif
