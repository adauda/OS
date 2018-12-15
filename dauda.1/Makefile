CC	= gcc
CFLAGS	= -g
TARGET	= ass1
OBJS	= chain.o

$(TARGET):	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

chain.o:	chain.c
	$(CC) $(CFLAGS) -c chain.c

clean:
	rm -f *.o $(TARGET) 
