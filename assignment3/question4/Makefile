INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= 
CFILES= posix_message_queue.c 

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all: posix_message_queue 

clean:
	-rm -f *.o *.d
	-rm -f perfmon pthread

distclean:
	-rm -f *.o *.d
	-rm -f pthread

posix_message_queue:posix_message_queue.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lrt -Wall -Werror -pedantic

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
