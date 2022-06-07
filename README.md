# Project-SO

Project for operating systems class

## Grade

Final grade of the project was **99%** out of **100%**

## Intro

This project is an offload simulator for the class of _Operating systems_.

## How to run

### Download

Download files to where you want to run the project.

### Compilation

To compile the project, open a console and navigate to the project folder, and then run the command `make all` to compile the entire project.
There will be two _executable_ files which are both executables needed.

### Running

To run the project, firstly run `./offload_simulator {config_name}`, providing a config file (also available in the project files), to use the already supplied configuration file the command `./offload_simulator config.txt`. The server should start, load the configuration and create all processes and threads required by it.  
The second part of the project is the `./mobile_node {# of requests to generate} {interval between requests (ms)} {MIPS} {maximum execution time}`. This will start a mobile node, which will send requests to the **Offload Simulator** which will process them.

_Note: for the project's intermidiate goal, the connection between mobile nodes and the offload simulator will not be done. Mobile node will only print it's inputs, and the offload simulator will only start the processes and threads required later._

### Signals

##### SIGINT

-   **System Manager** - Logs "Command Received" and Starts shutdown procedure

##### SIGTSTP

-   **System Manager** - Logs "Command Received" and prints out statistics

##### SIGUSR1

-   **Maintenance Manager** - Starts Shutdown Procedure
-   **System Manager** - Starts Shutdown Procedure
-   **Task Manager** - Starts Shutdown Procedure
-   **Edge Server** - Starts Shutdown Procedure
-   **Monitor** - Starts Shutdown Procedure

How the signals are sent:  
Task Manager ==(kill)=> System Manager  
System Manager ==(kill)=> Maintenance Manager  
System Manager ==(kill)=> Task Manager  
System Manager ==(kill)=> Monitor  
Task Manager ==(kill)=> Edge Server

##### SIGUSR2

-   **System Manager** - Print out statistics

How the signals are sent:  
Task Manager ==(kill)=> System Manager
