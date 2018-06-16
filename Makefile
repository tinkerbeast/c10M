CC = gcc
override CFLAGS += -Wall -Wextra -Werror -g -c
override LDFLAGS += -lpthread
CFILE = *.c
LINT_MODE = weak

all: httpio

httpio: main.o tuple.o poll.o handler.o server.o
	$(CC) main.o tuple.o poll.o handler.o server.o -o httpio $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) main.c -o main.o

tuple.o: tuple_socket_inet.c tuple.h
	$(CC) $(CFLAGS) tuple_socket_inet.c -o tuple.o

poll.o: poll.c poll.h
	$(CC) $(CFLAGS) poll.c -o poll.o

handler.o: handler.c handler.h
	$(CC) $(CFLAGS) handler.c -o handler.o

server.o: server.c server.h
	$(CC) $(CFLAGS) server.c -o server.o

clean:
	rm -f *.o httpio

echo:
	@echo "CC:$(CC), CFLAGS:$(CFLAGS), LDFLAGS:$(LDFLAGS), CFILE:$(CFILE), LINT:$(LINT_MODE)"

style:
	cppcheck --enable=all $(CFILE)

lint:
	splint +posixlib -I/usr/include/x86_64-linux-gnu -$(LINT_MODE) $(CFILE)
