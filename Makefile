#Author: Tyler Hackett
#Pledge: I pledge my honor that I have abided by the Stevens Honor System.

CC = gcc
CFLAGS = -g -std=c17 -fmessage-length=0

all: client server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

server: server.c
	$(CC) $(CFLAGS) server.c -o server

clean:
	rm -f client server