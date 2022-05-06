// José Miguel Saraiva Monteiro - 2015235572

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "logger.h"
#include "mobile_node.h"
#include "system_manager.h"

#define CONVERT_NS_TO_MS (1000000)

// ? >> How to use mobile_node <<
// ? $ mobile_node {nº pedidos a gerar} {intervalo entre pedidos em ms}
// ? {milhares de instruções de cada pedido} {tempo máximo para execução}

int main(int argc, char* argv[]) {
    int i, request_number, request_interval, request_instructions, max_execute_time;
    int fd_pipe;
    char message_to_send[LOG_MESSAGE_SIZE / 2];
    struct timespec remaining, request = {0, 0};
    pid_t mobile_node_pid;
    
    if (argc > 5) {
        bad_arguments("Too many arguments!");
        exit(-1);
    }
    if (argc < 5) {
        bad_arguments("Missing argument(s)!");
        exit(-1);
    }

    request_number = atoi(argv[1]);
    request_interval = atoi(argv[2]);
    request_instructions = atoi(argv[3]);
    max_execute_time = atoi(argv[4]);

    // Validate params
    if (!request_number || !request_interval || !request_instructions || !max_execute_time) {
        bad_arguments("Invalid param!");
        exit(-1);
    }
    // #ifdef DEBUG
    printf("I'm a mobile node!\nMy input params are as follow:\n");
    printf("req_number = %d\nreq_interval (ms) = %d\n", request_number, request_interval);
    printf("req_instructions (MIPS) = %d\nmax_exec_time = %d\n", request_instructions, max_execute_time);
    // #endif
    
    // * Logic from here 
    mobile_node_pid = getpid();

    request.tv_nsec = request_interval * CONVERT_NS_TO_MS;

    // Open pipe
    if ((fd_pipe = open(PIPE_NAME, O_RDWR)) < 0) {
        perror("Can't open the pipe!");
        exit(0);
    }

    // Generate requests
    for (i = 0; i < request_number; i++) {
        // TODO: Request struct
        // Populate message "data"
        snprintf(message_to_send, LOG_MESSAGE_SIZE / 2, "%d-%d:%d:%d", mobile_node_pid, i, request_instructions, max_execute_time);
        
        write(fd_pipe, &message_to_send, sizeof(message_to_send));

        #ifdef DEBUG
        printf("Sent request #%d, sleeping for %dms!\n", i + 1, request_interval);
        #endif
        nanosleep(&request, &remaining);
    }

    close(fd_pipe);

    return 0;
}

void bad_arguments(char error_message[]) {
    printf("%s\n", error_message);
    printf("Correct command usage:\n");
    printf("$ mobile_node {nº pedidos a gerar} {intervalo entre pedidos em ms} {milhares de instruções de cada pedido} {tempo máximo para execução}\n");
}
