// José Miguel Saraiva Monteiro - 2015235572

#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <wait.h>

#include "task_manager.h"
#include "system_manager.h"
#include "logger.h"
#include "edge_server.h"

#define EXIT_COMMAND "EXIT"
#define STATS_COMMAND "STATS"

pthread_t scheduler_id;
pthread_t dispatcher_id;
pthread_mutex_t task_manager_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t new_task_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t terminate_task_mngr_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t new_task_cond = PTHREAD_COND_INITIALIZER;
fd_set read_set;

int is_program_running = 1;
char pipe_string[(LOG_MESSAGE_SIZE / 2)];
int *unnamed_pipes;
static int server_number;
static int fastest_server_mips = -1;

void task_manager() {
    int i;
    int char_number;
    char log_message[LOG_MESSAGE_SIZE];
    char *pipe_task_message = NULL; // used to split the task message
    task_struct task_received;
    pid_t system_manager_pid = getppid();
    handle_log("INFO: Task Manager Started");

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_task_mngr_shutdown);
    sem_wait(mutex_config);

    program_configuration->task_manager_pid = getpid();
    server_number = program_configuration->edge_server_number;

    sem_post(mutex_config);

    unnamed_pipes = (int*) malloc(sizeof(int) * (2 * server_number));

    // Create one unnamed pipe for each server process
    for (i = 0; i < server_number; i++) {
        pipe(&unnamed_pipes[i * 2]);
    }

    // Create Edge servers
    for (i = 0; i < server_number; i++) {
        // fork() == 0 => Child
        if (fork() == 0) {
            start_edge_server(&servers[i], i, server_number, unnamed_pipes);
        }
        if (fastest_server_mips == -1 || 
                    (fastest_server_mips < servers[i].v_cpu1 || 
                     fastest_server_mips < servers[i].v_cpu2)) {
            fastest_server_mips = servers[i].v_cpu1 > servers[i].v_cpu2
                                ? servers[i].v_cpu1 
                                : servers[i].v_cpu2;
        }
    }
    #ifdef DEBUG
    printf("[TASK MANAGER] SLOWEST SERVER MIPS = %d\n", fastest_server_mips);
    #endif

    for (i = 0; i < server_number; i++) {
        close(unnamed_pipes[(i * 2)]); // 0 = READ
    }
    
    pthread_create(&scheduler_id, NULL, scheduler, 0);
    pthread_create(&dispatcher_id, NULL, dispatcher, 0);

    #ifdef DEBUG
    printf("\t\tTask Manager PID = %d\n", getpid());

    printf("Edge server PIDs:\n");
    sem_wait(mutex_servers);
    for(int i = 0; i < server_number; i++) {
        printf("Server #%d PID = %d\n", i + 1, servers[i].server_pid);
    }
    sem_post(mutex_servers);
    #endif

    while (1) {
        // FD_ZERO(&read_set);

        // FD_SET(fd_task_pipe, &read_set);    // ? Set named pipe file descriptor

        // if (select(fd_task_pipe + 1, &read_set, NULL, NULL, NULL) > 0) {

            // if (FD_ISSET(fd_task_pipe, &read_set)) {
        // printf("[Task Mngr - Pipe Reader] Waiting for new reading\n");
        char_number = read(fd_task_pipe, pipe_string, sizeof(pipe_string));
        pipe_string[char_number - 1] = '\0'; // ? Put a \0 at the string end

        if (index(pipe_string, ':') != NULL) {
            pipe_task_message = strtok(pipe_string, ":");
            strcpy(task_received.task_id, pipe_task_message);

            pipe_task_message = strtok(NULL, ":");
            task_received.mips = atoi(pipe_task_message);
            
            pipe_task_message = strtok(NULL, ":");
            task_received.exec_time = atoi(pipe_task_message);

            task_received.priority = -1;

            add_task_to_queue(task_received);

            #ifdef DEBUG
            printf("\t[TASK MNGR]Received task: ID: %s | %d MIPS | %d Exec time\n", 
                    task_received.task_id, 
                    task_received.mips, 
                    task_received.exec_time);
            #endif
        }
        else if (strcmp(pipe_string, STATS_COMMAND) == 0) {
            // * Signal System manager to print stats
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                    "COMMAND: Received command: \'%s\'", 
                    pipe_string);
            handle_log(log_message);
            kill(system_manager_pid, SIGUSR2);
        }
        else if (strcmp(pipe_string, EXIT_COMMAND) == 0) {
            // * Signal System manager to shut down
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                    "COMMAND: Received command: \'%s\'", 
                    pipe_string);
            handle_log(log_message);
            kill(system_manager_pid, SIGUSR1);
        } 
        else {
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                    "COMMAND: Received Unknown command: \'%s\'", 
                    pipe_string);
            handle_log(log_message);
        }
            // }
        // }
    }
}

void add_task_to_queue(task_struct task) {
    char log_message[LOG_MESSAGE_SIZE];
    // Timestamps task with time it was added to queue
    task.arrived_at = time(NULL); 

    #ifdef DEBUG
    printf("\t[SCHEDULER] RECEIVED TASK: %s:%d:%d\n", task.task_id, task.mips, task.exec_time);
    #endif

    sem_wait(mutex_tasks);
    if (task_queue->occupied_positions == task_queue->size) {
        // Drop task + Log it was dropped
        snprintf(log_message, LOG_MESSAGE_SIZE, 
                "WARNING: Task with ID: \'%s\' dropped, task queue full!", 
                task.task_id);
        handle_log(log_message);
        sem_post(mutex_tasks);
        // Even if queue is full, check if all tasks are still valid
        pthread_mutex_lock(&new_task_mutex);
        pthread_cond_broadcast(&new_task_cond);
        pthread_mutex_unlock(&new_task_mutex);
        return;
    }

    task_queue->task_list[task_queue->occupied_positions] = task; // append at the end
    task_queue->occupied_positions++;

    sem_post(mutex_tasks);

    pthread_mutex_lock(&new_task_mutex);
    pthread_cond_broadcast(&new_task_cond);
    pthread_mutex_unlock(&new_task_mutex);

    pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
    pthread_cond_signal(&program_configuration->monitor_task_scheduled);
    pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);
}


// ! Make sure mutex is active before function call 
void remove_task_at_index(int index) {
    int i, j;

    #ifdef DEBUG
    printf("\t[TASK MNGR] Removed task \'%s - %d MIPS - %d max_time\'\n", 
                task_queue->task_list[index].task_id, 
                task_queue->task_list[index].mips, 
                task_queue->task_list[index].exec_time);
    #endif

    for (i = index, j = index + 1; i < (task_queue->occupied_positions); i++, j++) {
        task_queue->task_list[i] = task_queue->task_list[j];
        // if (i >= task_queue->occupied_positions) {
        //     task_queue->task_list[i].priority = -1;
        // }
    }
}

void update_task_priorities() {
    int i, task_mips, task_execution_time, task_min_finish_time;
    time_t current_time, task_arrival;
    char log_message[LOG_MESSAGE_SIZE];

    current_time = time(NULL);

    sem_wait(mutex_tasks);
    for (i = 0; i < task_queue->occupied_positions; i++) {
        task_mips = task_queue->task_list[i].mips;
        task_arrival = task_queue->task_list[i].arrived_at;
        task_execution_time = task_queue->task_list[i].exec_time;
        task_min_finish_time = (current_time + (task_mips / fastest_server_mips 
                                + (task_mips % fastest_server_mips ? 1 : 0)));
        
        // * condition: current time - arrived_at > exec_time => Task exec time has expired
        if (current_time - task_arrival > task_execution_time) {
            // Remove task from queue - Execution time has passed
            snprintf(log_message, LOG_MESSAGE_SIZE, 
                    "WARNING: task \'%s\' removed - Exceeded execution time", 
                    task_queue->task_list[i].task_id);
            remove_task_at_index(i);
            task_queue->occupied_positions--; // Freed one spot
            #ifdef DEBUG
            printf("[UPDATER] Deleted a task at %d!\n", i);
            #endif
            if (i > 0) {
                i--;
            }
        }
        else {
            task_queue->task_list[i].priority = task_min_finish_time 
                                                - (task_arrival + task_execution_time) >= 1 ? 
                                                    task_min_finish_time 
                                                    - (task_arrival + task_execution_time) 
                                                    : 1;
        }
    }
    sem_post(mutex_tasks);
}

void handle_task_mngr_shutdown(int signum) {
    int i, server_pid;
    char log_missing_tasks[LOG_MESSAGE_SIZE];
    
    #ifdef DEBUG
    if (signum == SIGUSR1) {
        printf("Task Manager Received signal \"SIGUSR1\"\n");
    }
    else {
        printf("Task Manager Received signal \"UNKOWN\"\n");
    }
    printf("Shutting down task Manager <=> %d\n", getpid());
    #endif
    
    is_program_running = 0;

    for(i = 0; i < server_number; i++) {
        #ifdef DEBUG
        printf("Waiting for edge server %d\n", i + 1);
        #endif
        sem_wait(mutex_servers);
        server_pid = servers[i].server_pid;
        sem_post(mutex_servers);

        kill(server_pid, SIGUSR1);
        
        waitpid(server_pid, NULL, 0);
        #ifdef DEBUG
        printf("Edge server %d Finished\n", i + 1);
        #endif
    }

    #ifdef DEBUG
    printf("Servers finished, signalling cond var | Terminate Task MNGR\n");
    #endif
    
    pthread_mutex_lock(&task_manager_thread_mutex);
    pthread_cond_broadcast(&terminate_task_mngr_cond);
    pthread_mutex_unlock(&task_manager_thread_mutex);
    
    // printf("TASK MANAGER => \tPRE \"LOCK\"\n");
    pthread_mutex_lock(&new_task_mutex);
    // printf("TASK MANAGER => \tPRE \"BROADCAST\"\n");
    pthread_cond_broadcast(&new_task_cond);
    // printf("TASK MANAGER => \tPRE \"UNLOCK\"\n");
    pthread_mutex_unlock(&new_task_mutex);

    pthread_mutex_lock(&program_configuration->server_available_for_task_mutex);
    pthread_cond_broadcast(&program_configuration->server_available_for_task);
    pthread_mutex_unlock(&program_configuration->server_available_for_task_mutex);

    pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
    pthread_cond_broadcast(&program_configuration->monitor_task_scheduled);
    pthread_cond_broadcast(&program_configuration->monitor_task_dispatched);
    pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);

    #ifdef DEBUG
    printf("TASK MANAGER => \tPRE \"Join Threads\"\n");
    #endif
    
    // printf("TASK MANAGER => \tPRE \"Join Scheduler\"\n");
    pthread_join(scheduler_id, NULL);
    // printf("TASK MANAGER => \tPRE \"Join Dispatcher\"\n");
    pthread_join(dispatcher_id, NULL);

    #ifdef DEBUG
    printf("TASK MANAGER => \tPOST \"Join Threads\"\n");
    printf("Exiting from task Manager\n");
    #endif

    pthread_mutex_destroy(&task_manager_thread_mutex);
    pthread_mutex_destroy(&new_task_mutex);

    for(i = 0; i < server_number * 2; i++) {
        close(unnamed_pipes[i]);
    }
    
    sem_wait(mutex_tasks);
    handle_log("INFO: Tasks not executed:");
    for (i = 0; i < task_queue->occupied_positions; i++) {
        sprintf(log_missing_tasks, "INFO: -> ID: \'%s\' | MIPS = %d | Exec time = %d", 
                                task_queue->task_list[i].task_id,
                                task_queue->task_list[i].mips,
                                task_queue->task_list[i].exec_time);
        handle_log(log_missing_tasks);
    }

    sem_wait(mutex_stats);
    program_stats->total_tasks_not_executed += task_queue->occupied_positions;
    sem_post(mutex_stats);
    sem_post(mutex_tasks);

    free(unnamed_pipes);
    exit(0);    
}

void* scheduler(void *p) {
    handle_log("INFO: Scheduler Started");

    #ifdef DEBUG
    printf("I'm a thread! my PID => %lu\t[SCHEDULER]\n", scheduler_id);
    #endif

    while (is_program_running) {
        pthread_mutex_lock(&new_task_mutex);
        pthread_cond_wait(&new_task_cond, &new_task_mutex);
        pthread_mutex_unlock(&new_task_mutex);

        if (!is_program_running) {
            break;
        }
        #ifdef DEBUG
        printf("[SCHEDULER] I've received a new task!\n");
        #endif
        
        update_task_priorities();        
        
    }
    
    #ifdef DEBUG
    printf("\t\tSCHEDULER - Unlocking mutex\n");
    printf("\t\tExiting thread: SCHEDULER\n");
    #endif

    pthread_exit(NULL);
}

void* dispatcher(void *p) {
    char task_name[TASK_ID_SIZE];
    int task_mips, task_exec;
    char message_to_send[LOG_MESSAGE_SIZE / 2];
    int i, highest_priority_task_index, highest_priority_task;
    time_t task_arrival, current_time;
    int task_min_finish_time, task_execution_time;
    int server_index = 0;
    handle_log("INFO: Dispatcher Started");

    #ifdef DEBUG
    printf("I'm a thread! my PID => %lu\t[DISPATCHER]\n", dispatcher_id);
    #endif

    // TODO: if (!servers_available) => condition variable
    // TODO: select task with highest priority (and can be done in the time expected)
    // TODO: send task to edge server
    // TODO: remove task from array
    // TODO: go back to start

    while(is_program_running) {
        server_index = -1;
        sem_wait(mutex_servers);
        for (i = 0; i < server_number; i++) {
            if (servers[i].available_for_tasks > 0) {
                server_index = i;
                break;
            }
        }
        sem_post(mutex_servers);

        if (server_index == -1) {
            // TODO: Condition Var
            pthread_mutex_lock(&program_configuration->server_available_for_task_mutex);
            pthread_cond_wait(&program_configuration->server_available_for_task, 
                                &program_configuration->server_available_for_task_mutex);
            pthread_mutex_unlock(&program_configuration->server_available_for_task_mutex);
            continue;
        }

        highest_priority_task_index = -1;
        highest_priority_task = -1;

        sem_wait(mutex_tasks);
        // Iterate tasks
        if (!task_queue->occupied_positions) {
            sem_post(mutex_tasks);
            pthread_mutex_lock(&new_task_mutex);
            pthread_cond_wait(&new_task_cond, &new_task_mutex); // ! Wait new task
            pthread_mutex_unlock(&new_task_mutex);
            continue;
        } 
        current_time = time(NULL);
        for (i = 0; i < task_queue->occupied_positions; i++) {
            task_mips = task_queue->task_list[i].mips;
            task_arrival = task_queue->task_list[i].arrived_at;
            task_execution_time = task_queue->task_list[i].exec_time;
            task_min_finish_time = (current_time + (task_mips / fastest_server_mips 
                                + (task_mips % fastest_server_mips ? 1 : 0)));
            // * condition: if the time the fastest vcpu takes to execute the task + the current time
            // * are > exec time => task cannot be performed on time => Expired
            if ( task_min_finish_time > task_arrival + task_execution_time) {
                // if Task can't be done on time, remove it from queue
                remove_task_at_index(i);
                task_queue->occupied_positions--;
                i--;
                // printf("[DISPATCHER] Removed expired task\n");
                sem_wait(mutex_stats);
                program_stats->total_tasks_not_executed++;
                sem_post(mutex_stats);
            }
            if (highest_priority_task == -1 || task_queue->task_list[i].priority < highest_priority_task) {
                // Get the lowest priority number (higher priority) task
                highest_priority_task = task_queue->task_list[i].priority;
                highest_priority_task_index = i;
            }
        }

        if (highest_priority_task_index < 0 || highest_priority_task < 0) {
            sem_post(mutex_tasks);
            continue;
        }
        // printf("\t\t[DISPATCHER] Selected task: %s:%d:d", task_queue->task_list[])

        strcpy(task_name, task_queue->task_list[highest_priority_task_index].task_id);
        task_mips = task_queue->task_list[highest_priority_task_index].mips;
        task_exec = task_queue->task_list[highest_priority_task_index].exec_time;
        sprintf(message_to_send, "%s:%d:%d", task_name, task_mips, task_exec);
        // printf("[DISPATCHER] Writing a task to pipe\n");
        // printf("[TASK MANAGER] Server #%d ready for task | printing to pipe %d\n", server_index, (server_index * 2) + 1);
        // printf("[TASK MANAGER] Sending Task: %s:%d:%d\n", task_name, task_mips, task_exec);
        // ! DISPATCHING TASK
        write(unnamed_pipes[(server_index * 2) + 1], &message_to_send, sizeof(message_to_send));

        sem_wait(mutex_stats);
        program_stats->total_wait_time += (current_time - task_queue->task_list[highest_priority_task_index].arrived_at);
        sem_post(mutex_stats);

        task_queue->time_to_process_task = current_time - task_queue->task_list[highest_priority_task_index].arrived_at;

        remove_task_at_index(highest_priority_task_index);
        task_queue->occupied_positions--;
        
        sem_post(mutex_tasks);
        
        pthread_mutex_lock(&program_configuration->monitor_variables_mutex);
        pthread_cond_signal(&program_configuration->monitor_task_dispatched);
        pthread_mutex_unlock(&program_configuration->monitor_variables_mutex);

        sleep(1);
    }

    // printf("[DISPATCHER] Exiting!\n");

    // pthread_mutex_lock(&task_manager_thread_mutex);
    // while (is_program_running) {
    //     pthread_cond_wait(&terminate_task_mngr_cond, &task_manager_thread_mutex);
        
    //     #ifdef DEBUG
    //     printf("\t\tI'm IN THE CONDITION VARIABLE - DISPATCHER \n");
    //     #endif
    // }
    // pthread_mutex_unlock(&task_manager_thread_mutex);
    #ifdef DEBUG
    printf("\t\tDISPATCHER - Unlocking mutex\n");
    #endif
    
    #ifdef DEBUG
    printf("\t\tExiting thread: DISPATCHER\n");
    #endif
    
    pthread_exit(NULL);
}
