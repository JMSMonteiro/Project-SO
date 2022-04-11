// José Miguel Saraiva Monteiro - 2015235572

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "logger.h"
#include "maintenance_manager.h"
#include "system_manager.h"

/*
? Generates maintenance messages, get responses and manage maintnance
*/

void maintenance_manager() {
    handle_log("INFO: Maintenance Manager Started");


    sem_wait(mutex_config);

    program_configuration->maintenance_monitor_pid = getpid();

    sem_post(mutex_config);


}
