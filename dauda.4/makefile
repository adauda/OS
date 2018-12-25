CC	= gcc
CFLAGS	= -g 
LIBS += -lpthread -lrt -pthread
T1 = oss
T2 = user
OBJS1	= oss.o
OBJS2	= user.o

all: $(T1) $(T2)

$(T1): $(OBJS1)
	$(CC) -o $@ $(OBJS1) $(LIBS)

$(T2): $(OBJS2)
	$(CC) -o $@ $(OBJS2) $(LIBS)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) $<

.PHONY: clean

clean:
	/bin/rm -f log core *.o $(T1) $(T2)

