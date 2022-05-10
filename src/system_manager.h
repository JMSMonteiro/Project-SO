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
    int available_for_tasks;
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
    int simulator_shutting_down;
    pid_t monitor_pid;
    pid_t task_manager_pid;
    pid_t maintenance_manager_pid;
    pthread_cond_t change_performance_mode;
    pthread_cond_t server_available_for_task;
    pthread_cond_t monitor_task_dispatched;
    pthread_cond_t monitor_task_scheduled;
    pthread_condattr_t cond_attr;
    pthread_mutex_t change_performance_mode_mutex;
    pthread_mutex_t server_available_for_task_mutex;
    pthread_mutex_t monitor_variables_mutex;
    pthread_mutexattr_t mutex_attr;
} prog_config;

typedef struct {
    int total_tasks_executed;
    int total_tasks_not_executed;
    long total_wait_time;
    double avg_response_time; //tempo desde que a tarefa chegou até começar a ser executada
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
    int priority;
    time_t arrived_at;
} task_struct;

typedef struct {
    task_struct *task_list;
    int size;
    int occupied_positions;
    time_t newest_task_arrival;
    // TODO: use this ^
} tasks_queue_info;

extern sem_t *mutex_logger;
extern sem_t *mutex_config;
extern sem_t *mutex_servers;
extern sem_t *mutex_stats;
extern sem_t *mutex_tasks;
extern key_t shmkey;
extern int shmid;
extern int fd_task_pipe;
extern int message_queue_id;
extern prog_config *program_configuration;
extern edge_server *servers;
extern statistics *program_stats;
extern tasks_queue_info *task_queue;

void signal_initializer();
void handle_display_stats(int signum);
void display_stats();
void start_semaphores();
void handle_program_finish(int signum);
void start_shutdown_routine(int signum);
void load_config(char *file_name);

#endif
