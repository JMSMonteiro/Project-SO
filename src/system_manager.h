// José Miguel Saraiva Monteiro - 2015235572

#ifndef _system_manager_h__
#define _system_manager_h__

#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>

// #define DEBUG   // Remove/Comment this line to remove debug messages
#define PIPE_NAME "TASK_PIPE"
#define TASK_ID_SIZE (15)

typedef struct {
    char name[64];
    int v_cpu1;
    int v_cpu2;
    int performance_mode;
    int is_shutting_down;
    int tasks_executed;
    int maintenance_operation_performed;
    pid_t server_pid;
    pthread_cond_t edge_stopped;
    pthread_condattr_t edge_cond_attr;
    pthread_mutex_t edge_server_mutex;
    pthread_mutexattr_t edge_mutex_attr;
} edge_server;

typedef struct {
    int queue_pos;
    int max_wait;
    int edge_server_number;
    int servers_fully_booted;
    int current_performance_mode; // 0 = Stopped | 1 = Normal | 2 = High Performance
    pid_t monitor_pid;
    pid_t task_manager_pid;
    pid_t maintenance_manager_pid;
    pthread_cond_t change_performance_mode;
    pthread_condattr_t cond_attr;
    pthread_mutex_t change_performance_mode_mutex;
    pthread_mutexattr_t mutex_attr;
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

typedef struct {
    char task_id[TASK_ID_SIZE];
    int mips;
    int exec_time;
} task_struct;

extern sem_t *mutex_logger;
extern sem_t *mutex_config;
extern sem_t *mutex_servers;
extern sem_t *mutex_stats;
extern key_t shmkey;
extern int shmid;
extern int fd_task_pipe;
extern int message_queue_id;
extern prog_config *program_configuration;
extern edge_server *servers;
extern statistics *program_stats;

void signal_initializer();
void handle_display_stats(int signum);
void display_stats();
void start_semaphores();
void handle_program_finish(int signum);
void start_shutdown_routine(int signum);
void load_config(char *file_name);

#endif
