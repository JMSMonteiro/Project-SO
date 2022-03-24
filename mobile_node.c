#include "mobile_node.h"
#include "system_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

// ? >> How to use mobile_node <<
// ? $ mobile_node {nº pedidos a gerar} {intervalo entre pedidos em ms}
// ? {milhares de instruções de cada pedido} {tempo máximo para execução}

int main(int argc, char* argv[]) {
    int request_number, request_interval, request_instructions, max_execute_time;    
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

    if (!request_number || !request_interval || !request_instructions || !max_execute_time) {
        bad_arguments("Arguments cannot be null!");
        exit(-1);
    }
    #ifdef DEBUG
    printf("I'm a mobile node!\nMy input params are as follow:\n");
    printf("req_number = %d\nreq_interval = %d\n", request_number, request_interval);
    printf("req_instructions = %d\nmax_exec_time = %d", request_instructions, max_execute_time);
    #endif
    return 0;
}

void bad_arguments(char error_message[]) {
    printf("%s\n", error_message);
    printf("Correct command usage:\n");
    printf("$ mobile_node {nº pedidos a gerar} {intervalo entre pedidos em ms} {milhares de instruções de cada pedido} {tempo máximo para execução}\n");
}