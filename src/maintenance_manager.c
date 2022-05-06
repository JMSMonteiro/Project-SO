// José Miguel Saraiva Monteiro - 2015235572

#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>

#include "logger.h"
#include "maintenance_manager.h"
#include "system_manager.h"

#define MAX_MAINTENANCE_TIME 5
#define MIN_MAINTENANCE_TIME 1

sem_t maintenance_limiter;
pthread_t *maint_mngr_threads;
int *server_indexes;
static int server_number;
static int booted_servers;
int maintenance_manager_running = 1;

/*
? Generates maintenance messages, get responses and manage maintnance
*/

void maintenance_manager() {
    int i;
    handle_log("INFO: Maintenance Manager Started");

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, handle_maintenance_mngr_shutdown);

    sem_wait(mutex_config);

    program_configuration->maintenance_manager_pid = getpid();
    server_number = program_configuration->edge_server_number;
    booted_servers = program_configuration->servers_fully_booted;

    sem_post(mutex_config);

    maint_mngr_threads = malloc(sizeof(pthread_t) * server_number);
    server_indexes = malloc(sizeof(int) * server_number);

    sem_init(&maintenance_limiter, 0, (server_number - 1));

    #ifdef DEBUG
    printf("[MAINTENANCE] waiting for servers to start\n");
    #endif
    // ! Block here - wait for all edge servers up and running
    pthread_mutex_lock(&program_configuration->change_performance_mode_mutex);
    while (server_number != booted_servers) {
        pthread_cond_wait(&program_configuration->change_performance_mode, &program_configuration->change_performance_mode_mutex);
        sem_wait(mutex_config);
        booted_servers = program_configuration->servers_fully_booted;
        sem_post(mutex_config);
    }
    pthread_mutex_unlock(&program_configuration->change_performance_mode_mutex);

    #ifdef DEBUG
    printf("[MAINTENANCE] Starting maintenance threads, #%d Servers booted!\n", booted_servers);
    #endif

    for (i = 0; i < server_number; i++) {
        server_indexes[i] = i;
        pthread_create(&maint_mngr_threads[i], NULL, maintenance_thread, &server_indexes[i]);
    }

    srand(getpid());

    while(1) {
        // * Maintenance Interval || Maintenance time => Random [1 - 5] seconds
        // ? mngr => Server -> Going to maintenance ops
        // ? Server => mngr -> Ready for intervention
        // ? Server = Stopped
        // ? mngr => Server -> Restart working
    }    
    exit(0);
}

void handle_maintenance_mngr_shutdown(int signum) {
    int i;
    #ifdef DEBUG
    if (signum == SIGUSR1) {
        printf("Maintenance Manager received signal \"SIGUSR1\"\n");
    }
    printf("Shutting down Maintenance Manager\n");
    #endif

    maintenance_manager_running = 0;

    for (i = 0; i < server_number; i++) {
        pthread_join(maint_mngr_threads[i], NULL);
    }

    free(maint_mngr_threads);
    free(server_indexes);


    exit(0);
}

void *maintenance_thread(void* server_ind) {
    int server_index = *((int *) server_ind);
    int maintenance_time;       // Time which a maintenance operation takes
    int maintenance_free_time;  // Time for which a server doesn't need maintenance
    struct timespec remaining, request = {0, 0};
    maintenance_message message;

    // * Server index has to be +1 to avoid msgrcv for index '0' to catch everything
    message.server_index = (server_index + 1);

    #ifdef DEBUG
    printf("\t\tI'm a Maintenance mngr thread for Server %d\n", server_index);
    #endif

    while (maintenance_manager_running) {
        
        maintenance_free_time = (rand() % (MAX_MAINTENANCE_TIME - MIN_MAINTENANCE_TIME + 1)) + MIN_MAINTENANCE_TIME;
        
        #ifdef DEBUG
        printf("\t\t\tMaintenance Manager [SERVER %d]: Next maintenance in %ds\n", server_index, maintenance_free_time);
        #endif
        
        // ? Determines how many seconds the server can work without maintenance
        request.tv_sec = maintenance_free_time;
        // ? Sleep for: maintenance_free_time seconds
        nanosleep(&request, &remaining); // * Server works for n seconds
        
        // ? Generate random interval for server maintenance time
        maintenance_time = (rand() % (MAX_MAINTENANCE_TIME - MIN_MAINTENANCE_TIME + 1)) + MIN_MAINTENANCE_TIME;
        request.tv_sec = maintenance_time;
        
        // ? Guarantees that at least one server is running
        sem_wait(&maintenance_limiter);
        
        // TODO: GENERATE MESSAGE TO ALERT SERVER FOR MAINTENANCE
        message.stop_flag = 1;
        message.maintenance_time = maintenance_time;
        msgsnd(message_queue_id, &message, sizeof(maintenance_message) - sizeof(long), 0);

        // TODO: WAIT FOR SERVER TO BE ON "STOPPED" MODE
        
        #ifdef DEBUG
        printf("\t\t\tMaintenance Manager [SERVER %d]: Maintaining for %ds\n", server_index, maintenance_time);
        #endif
        sem_wait(mutex_servers);
        if (servers[server_index].is_shutting_down) {
            sem_post(mutex_servers);
            sem_post(&maintenance_limiter);
            break;
        }
        sem_post(mutex_servers);
        
        nanosleep(&request, &remaining);

        // TODO: GENERATE MESSAGE TO ALERT SERVER TO RESUME WORK
        message.stop_flag = 0;
        msgsnd(message_queue_id, &message, sizeof(maintenance_message) - sizeof(long), 0);


        // ? The server being mantained is going to work
        // ? so another can go do maintnance tasks
        sem_post(&maintenance_limiter);
        
    }

    pthread_exit(NULL);
}
