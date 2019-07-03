CC = gcc
override CFLAGS += -Wall -Wextra -Werror -g -c -Isrc/httpio
override LDFLAGS += -lpthread
CFILE = *.c
LINT_MODE = weak

all: httpio

httpio: main.o tuple.o poll.o handler.o server.o jobpool.o
	$(CC) main.o tuple.o poll.o handler.o server.o jobpool.o -o httpio $(LDFLAGS)

main.o: src/main.c
	$(CC) $(CFLAGS) src/main.c -o main.o

tuple.o: src/httpio/tuple_socket_inet.c src/httpio/tuple.h
	$(CC) $(CFLAGS) src/httpio/tuple_socket_inet.c -o tuple.o

poll.o: src/httpio/poll.c src/httpio/poll.h
	$(CC) $(CFLAGS) src/httpio/poll.c -o poll.o

handler.o: src/httpio/handler.c src/httpio/handler.h
	$(CC) $(CFLAGS) src/httpio/handler.c -o handler.o

server.o: src/httpio/server.c src/httpio/server.h
	$(CC) $(CFLAGS) src/httpio/server.c -o server.o

jobpool.o: src/httpio/jobpool.c src/httpio/jobpool.h
	$(CC) $(CFLAGS) src/httpio/jobpool.c -o jobpool.o

clean:
	rm -f *.o httpio

echo:
	@echo "CC:$(CC), CFLAGS:$(CFLAGS), LDFLAGS:$(LDFLAGS), CFILE:$(CFILE), LINT:$(LINT_MODE)"

style:
	cppcheck --enable=all $(CFILE)

lint:
	splint +posixlib -I/usr/include/x86_64-linux-gnu -$(LINT_MODE) $(CFILE)
