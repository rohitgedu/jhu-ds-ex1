CC=gcc

CFLAGS = -ansi -c -Wall -pedantic

all: t_rcv t_ncp rcv ncp

t_rcv: t_rcv.o
	$(CC) -o t_rcv t_rcv.o
t_ncp: t_ncp.o
	$(CC) -o t_ncp t_ncp.o
rcv: rcv.o
	$(CC) -o rcv rcv.o
ncp: ncp.o
	$(CC) -o ncp ncp.o

clean:
	rm *.o
	rm t_rcv t_ncp rcv t_rcv

%.o: %.c
	$(CC) $(CFLAGS) $*.c


