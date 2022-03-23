
FLAGS	= -g -Wall -pthread
CC		= gcc
PROG	= offload_simulator.out
OBJS	= system_manager.o logger.o

all:	${PROG}

clean:
	rm ${OBJS} *~ ${PROG}

${PROG}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} -c $< -o $@

##################

system_manager.o:	system_manager.c system_manager.h logger.h 

logger.o:	logger.h logger.c

