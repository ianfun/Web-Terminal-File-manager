#CC=clang
LLHTTP_OPTS=-O3 -Wall -Wextra -Wno-unused-parameter
SERVER_OPTS=-O3 -Wall -Wextra -Wno-unused-parameter # -g -fsanitize=address

build:
	$(CC) $(SERVER_OPTS) server.c api.o http.o llhttp.o sha1.o -o server -luring -lutil
run:
	./server 0.0.0.0 8000 ./static
debug:
	gdb --args ./server 0.0.0.0 8000 ./static
clean:
	rm server *.o
llhttp:
	$(CC) $(LLHTTP_OPTS) -c api.c
	$(CC) $(LLHTTP_OPTS) -c http.c
	$(CC) $(LLHTTP_OPTS) -c llhttp.c

all: build

