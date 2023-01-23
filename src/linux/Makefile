CC=clang
LLHTTP_OPTS=-Os -g3 -Wall -Wextra -Wno-unused-parameter
SERVER_OPTS=-Os -g3 -Wall -Wextra -Wno-unused-parameter

build:
	$(CC) $(SERVER_OPTS) server.c api.o http.o llhttp.o sha1.o -o server -luring -lutil
run:
	./server 0.0.0.0 8000 ./static
clean:
	rm server *.o
llhttp:
	$(CC) $(LLHTTP_OPTS) -c api.c
	$(CC) $(LLHTTP_OPTS) -c http.c
	$(CC) $(LLHTTP_OPTS) -c llhttp.c

all: build
