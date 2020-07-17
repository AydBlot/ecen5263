INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lrt -lpthread

HFILES= sharpen.h
CFILES= capture_old.c sharpen.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	continuous_transform_capture

clean:
	-rm -f *.o *.d
	-rm -f continuous_transform_capture

distclean:
	-rm -f *.o *.d

continuous_transform_capture: capture_old.o sharpen.o
	      $(CC) $(CFLAGS) capture_old.o sharpen.o -o continuous_transform_capture $(LIBS)

capture: capture_old.c sharpen.h
	$(CC) $(CFLAGS) -c capture_old.c 

sharpen: sharpen.c sharpen.h
	$(CC) $(CFLAGS) -c sharpen.c 
	
