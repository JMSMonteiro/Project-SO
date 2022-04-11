// José Miguel Saraiva Monteiro - 2015235572

#include <stdlib.h>
#include <unistd.h>

#include "logger.h"
#include "system_manager.h"


/*
? The Monitor's job is to control the edge servers, according to the established rules
? prog_config => current_performance_mode
*/

void monitor() {
    handle_log("INFO: Monitor Started");

    sem_wait(mutex_config);

    program_configuration->monitor_pid = getpid();

    sem_post(mutex_config);


}
