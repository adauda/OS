CC=gcc
CFLAGS=-g

all: oss user

oss: oss.c
	gcc -lpthread -lrt -pthread -o oss oss.c

user: user.c
	gcc -lpthread -lrt -pthread -o user user.c

clean:
	rm oss user
