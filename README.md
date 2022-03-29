# Project-SO
Project for operating systems class

## Intro
This project is an offload simulator for the class of Operating systems.

## How to run
### Download
Download files to where you want to run the project.

### Compilation
To compile the project, open a console and navigate to the project folder, and then run the command `make all` to compile the entire project.
There will be two `.out` files which are both executables needed.

### Running
To run the project, firstly run `./offload_simulator.out {config_name}`, providing a config file (also available in the project files), to use the already supplied configuration file the command `./offload_simulator.out config.txt`. The server should start, load the configuration and create all processes and threads required by it.   
The second part of the project is the `./mobile_node {# of requests to generate} {interval between requests (ms)} {MIPS} {maximum execution time}`. This will start a mobile node, which will send requests to the **Offload Simulator** which will process them.  

*Note: for the project's intermidiate goal, the connection between mobile nodes and the offload simulator will not be done. Mobile node will only print it's inputs, and the offload simulator will only start the processes and threads required later.*
