// José Miguel Saraiva Monteiro - 2015235572

#ifndef _task_manager_h__
#define _task_manager_h__

#include "system_manager.h"

void task_manager();
void add_task_to_queue(task_struct task);
void update_task_queue_after_removal(int removed_index);
void update_task_priorities();
void handle_task_mngr_shutdown(int signum);
void *scheduler(void* p);
void *dispatcher(void* p);

#endif
