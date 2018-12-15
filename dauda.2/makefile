CC=gcc
CFLAGS=-g

all: master child

master: master.c
	gcc -lrt -o master master.c

child: child.c
	gcc -o child child.c

clean:
	rm master child *.o
