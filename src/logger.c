// José Miguel Saraiva Monteiro - 2015235572

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>

#include "logger.h"
#include "system_manager.h"

#define LOG_FILE "../logs/log.txt"

sem_t *mutex_logger;

void handle_log(char *message) {
    time_t seconds;
    struct tm *timeStruct = NULL;
    char full_log_message[LOG_MESSAGE_SIZE];

    // Get time
    seconds = time(NULL);
    
    // Properly format time
    timeStruct = localtime(&seconds);

    // Makes / Formats the full log message
    snprintf(full_log_message, LOG_MESSAGE_SIZE,
                                    "%02d:%02d:%02d %s",
                                    timeStruct->tm_hour, 
                                    timeStruct->tm_min, 
                                    timeStruct->tm_sec,
                                    message);

    sem_wait(mutex_logger);
    
    _print_to_stderr(full_log_message);
    _print_to_file(full_log_message);
    
    sem_post(mutex_logger);
}

void _print_to_stderr(char message[]) {
    fprintf(stderr, "%s\n", message);
}

void _print_to_file(char message[]) {
    FILE *qPtr;

    if ((qPtr = fopen(LOG_FILE, "a")) == NULL) {
        mkdir("../logs/", 0755);
        if ((qPtr = fopen(LOG_FILE, "a")) == NULL) {
            perror("Error Opening File, couldn't create directory\n");
        }
    }

    fprintf(qPtr, "%s\n", message);

    fclose(qPtr);
}
