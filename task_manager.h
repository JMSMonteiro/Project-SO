#ifndef _task_manager_h__
#define _task_manager_h__

#include "system_manager.h"

void task_manager(prog_config *config);
void *scheduler(void* p);

#endif