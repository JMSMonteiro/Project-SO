// José Miguel Saraiva Monteiro - 2015235572

#ifndef _logger_h__
#define _logger_h__

#include <time.h>

void handle_log(char message[]);
void _print_to_stderr(char message[]);
void _print_to_file(char message[]);

#endif
