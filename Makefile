CC=g++ -std=c++11

CFLAGS=-Wall -W -g -Werror 



all: client server

client: client.cpp raw.cpp
	$(CC) client.cpp raw.cpp $(CFLAGS) -o client

server: server.cpp 
	$(CC) server.cpp $(CFLAGS) -o server

clean:
	rm -f client server *.o

