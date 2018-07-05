CC=$(CROSS_COMPILE)gcc

CFLAGS+=-Wall -std=gnu11 -fPIC
LDFLAGS+=-lpthread

.PHONY: all

all: epollet_socket epollexclusive

.PHONY: debug

debug: CFLAGS += -O0 -DDEBUG -g -Wno-unused-variable
debug: all

epollet_socket: epollet_socket.o connection.o
	$(CC) $(CFLAGS) -o epollet_socket epollet_socket.o connection.o

epollexclusive: epollexclusive.o connection.o
	$(CC) $(CFLAGS) -o epollexclusive epollexclusive.o connection.o $(LDFLAGS)

epollet_socket.o: epollet_socket.c
	$(CC) $(CFLAGS) -c epollet_socket.c

epollexclusive.o: epollexclusive.c
	$(CC) $(CFLAGS) -c epollexclusive.c

connection.o: connection.c
	$(CC) $(CFLAGS) -c connection.c

clean:
	-rm epollet_socket epollet_socket epollexclusive *.o

