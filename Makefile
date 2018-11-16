#makefile for proxy server

CC = gcc
Flags = -Wall -g

proxy : server.c

	$(CC) $(Flags) server.c -o proxy

clean: 
	rm -f proxy


