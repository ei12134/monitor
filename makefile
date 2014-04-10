CC = gcc
PROG = monitor
CFLAGS = -Wall
SRCS = monitor.c
BIN_DIR = bin

all: bin monitor

bin:
	mkdir -p ${BIN_DIR}

monitor:
	cc $(SRCS) -o $(BIN_DIR)/$(PROG) $(CFLAGS)

clean:
	@rm -f $(BIN_DIR)/$(PROG) $(BIN_DIR)/*.o core && rm -f -r bin
