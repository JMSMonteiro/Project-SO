// José Miguel Saraiva Monteiro - 2015235572

#ifndef _mantenance_manager_h__
#define _mantenance_manager_h__

void maintenance_manager();
void handle_maintenance_mngr_shutdown(int signum);
void *maintenance_thread(void* server_ind);

#endif
