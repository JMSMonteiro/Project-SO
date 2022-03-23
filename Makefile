
FLAGS	= -g -Wall -pthread
CC		= gcc
PROG	= offload_simulator.out
OBJS	= system_manager.o logger.o task_manager.o edge_server.o

all:	${PROG}

clean:
	rm ${OBJS} *~ ${PROG}

${PROG}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} -c $< -o $@

##################

system_manager.o:	system_manager.c system_manager.h logger.h task_manager.h

task_manager.o: task_manager.c task_manager.h system_manager.h  logger.h edge_server.h 

edge_server.o: edge_server.c edge_server.h system_manager.h

logger.o:	logger.h logger.c

