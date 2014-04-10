CC=gcc
PROG= monitor
CFLAGS= -I -Wall -DDEBUG
SRCS= monitor.c
monitor:
	cc $(SRCS) -o $(PROG) $(CFLAGS)


clean:
	@rm -f $(PROG) *.o core
