// José Miguel Saraiva Monteiro - 2015235572

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "logger.h"

#define LOG_FILE "log.txt"

void handle_log(char message[]) {
    time_t seconds;
    struct tm *timeStruct = NULL;
    char current_time[10] = {};
    char full_log_message[64] = {};

    // Get time
    seconds = time(NULL);
    
    // Properly format time
    timeStruct = localtime(&seconds);

    // Convert time values to string stored in current_time
    sprintf(current_time, "%02d:%02d:%02d ",timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec);

    strcat(full_log_message, current_time);
    strcat(full_log_message, message);

    _print_to_stderr(full_log_message);
    _print_to_file(full_log_message);
}

void _print_to_stderr(char message[]) {
    fprintf(stderr, "%s\n", message);
}

void _print_to_file(char message[]) {
    FILE *qPtr;

    if ((qPtr = fopen(LOG_FILE, "a")) == NULL) {
        perror("Error Opening File\n");
    }

    fprintf(qPtr, "%s\n", message);

    fclose(qPtr);
}
