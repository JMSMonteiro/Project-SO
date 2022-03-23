
FLAGS	= -g -Wall -pthread
CC		= gcc
PROG	= program.out
OBJS	= main.o logger.o

all:	${PROG}

clean:
	rm ${OBJS} *~ ${PROG}

${PROG}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} -c $< -o $@

##################

main.o:	main.h logger.h main.c

logger.o: logger.h logger.c

program:	main.o 