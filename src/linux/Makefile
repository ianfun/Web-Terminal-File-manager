CC=clang
LLHTTP_OPTS=-O3 -Wall -Wextra -Wno-unused-parameter
SHA1_OPTS=$(LLHTTP_OPTS)
SERVER_OPTS=-O3 -Wall -Wextra -Wno-unused-parameter -pthread # -g -fsanitize=address
LLVM_OPTS=-O3 -I /usr/include/llvm-15/ -Wall -Wextra -Wno-unused-parameter

build:
	$(CC) $(SERVER_OPTS) server.c -c
	$(CC)++ $(SERVER_OPTS) server.o -lpthread -luring -lutil api.o http.o llhttp.o sha1.o killer.o /usr/lib/llvm-15/lib/libLLVM-15.so -o server
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
pidset:
	$(CC)++ killer.cpp $(LLVM_OPTS) -c
sha1:
	$(CC) sha1.c $(SHA1_OPTS) -c
