// José Miguel Saraiva Monteiro - 2015235572

#ifndef _system_manager_h__
#define _system_manager_h__

#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>

// #define DEBUG   // Remove/Comment this line to remove debug messages

typedef struct {
    char name[64];
    int v_cpu1;
    int v_cpu2;
    int performance_mode;
    int can_accept_tasks;
} edge_server;

typedef struct {
    int queue_pos;
    int max_wait;
    int edge_server_number;
    int current_performance_mode;
} prog_config;

extern sem_t *mutex_logger;
extern int shmid;
extern key_t shmkey;
extern prog_config *program_configuration;
extern edge_server *servers;


void load_config(char *file_name);

#endif
