CFLAGS=-g -Wall -pedantic -lpthread

.PHONY: all
all: chat-server chat-client

chat-server: server.c
	gcc $(CFLAGS) -o $@ $^

chat-client: client.c
	gcc $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f chat-server chat-client

