// José Miguel Saraiva Monteiro - 2015235572

#ifndef _system_manager_h__
#define _system_manager_h__

#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>

// #define DEBUG   // Remove/Comment this line to remove debug messages
#define PIPE_NAME "TASK_PIPE"

typedef struct {
    char name[64];
    int v_cpu1;
    int v_cpu2;
    int performance_mode;
    int can_accept_tasks;
    int tasks_executed;
    int maintenance_operation_performed;
    pid_t server_pid;
} edge_server;

typedef struct {
    int queue_pos;
    int max_wait;
    int edge_server_number;
    int current_performance_mode;
    pid_t monitor_pid;
    pid_t task_manager_pid;
    pid_t maintenance_monitor_pid;
} prog_config;

typedef struct {
    int total_tasks_executed;
    int total_tasks_not_executed;
    int avg_response_time; //tempo desde que a tarefa chegou até começar a ser executada
} statistics;

typedef struct {
    long server_index; // * Index of the server in the SHM array
    // ? Insert Payload here
    int stop_flag;
    int maintenance_time;
} maintenance_message;

extern sem_t *mutex_logger;
extern sem_t *mutex_config;
extern sem_t *mutex_servers;
extern sem_t *mutex_stats;
extern int shmid;
extern int fd_task_pipe;
extern int message_queue_id;
extern key_t shmkey;
extern prog_config *program_configuration;
extern edge_server *servers;
extern statistics *program_stats;

void signal_initializer();
void display_stats();
void start_semaphores();
void handle_program_finish(int signum);
void load_config(char *file_name);

#endif
