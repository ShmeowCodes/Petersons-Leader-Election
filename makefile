# make file for leader election algorithm

OBJS	= main.o
CC  	= gcc -pthread

all: leader clean

main.o: main.c
	$(CC) -c main.c

leader: main.o
	$(CC) -o leader main.o

clean:
	rm  	$(OBJS)
