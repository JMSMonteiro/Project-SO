
FLAGS	= -g -Wall -pthread
CC		= gcc
SIMULATOR	= offload_simulator.out
MOBILE	= mobile_node.out
OBJS	= system_manager.o logger.o task_manager.o edge_server.o
MOBILE_DEP	= mobile_node.o

all:	${SIMULATOR} ${MOBILE}

clean:
	rm ${OBJS} *~ ${SIMULATOR}

${SIMULATOR}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

${MOBILE}:	${MOBILE_DEP}
	${CC} ${FLAGS} ${MOBILE_DEP} -o $@

.c.o:
	${CC} ${FLAGS} -c $< -o $@

##################

system_manager.o:	system_manager.c system_manager.h logger.h task_manager.h

task_manager.o: task_manager.c task_manager.h system_manager.h  logger.h edge_server.h 

edge_server.o: edge_server.c edge_server.h system_manager.h

logger.o:	logger.h logger.c

mobile_node.o: mobile_node.c mobile_node.h

