CC=clang
LLHTTP_OPTS=-Os -g3 -Wall -Wextra -Wno-unused-parameter
SERVER_OPTS=-Os -g3 -Wall -Wextra -Wno-unused-parameter -luring

build:
	$(CC) $(SERVER_OPTS) server.c api.o http.o llhttp.o -o server
run:
	./server
clean:
	rm server *.o
llhttp:
	$(CC) $(LLHTTP_OPTS) -c api.c
	$(CC) $(LLHTTP_OPTS) -c http.c
	$(CC) $(LLHTTP_OPTS) -c llhttp.c

all: build
