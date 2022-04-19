CFLAGS= -Wall -g

all: chat-server chat-client

chat-server: chat-server.c
	gcc $(CFLAGS) -pthread -o chat-server chat-server.c

chat-client: chat-client.c
	gcc $(CFLAGS) -pthread -o chat-client chat-client.c

.PHONY: clean
clean:
	rm -f chat-server chat-client
