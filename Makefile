all:	ddsync logdump
ddsync: main.c sender.c receiver.c common.h
	gcc -std=c99 -o ddsync main.c sender.c receiver.c MurmurHash3.c -lm -lnettle -I/usr/local/include -L/usr/local/lib -D_BSD_SOURCE
logdump: 
clean:
	rm ddsync logdump

