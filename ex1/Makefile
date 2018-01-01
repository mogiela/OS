CC = g++
CFLAGS = -Wall -std=c++11 -c -O0
LIBFLAGS = ar -rs 
OSMLIB = libosm.a

TAR = tar
TARFLAGS = -cvf
TARNAME = ex1.tar
TARSRCS = osm.c Makefile README

libosm.a: osm.o
	$(LIBFLAGS) $(OSMLIB) osm.o

osm.o: osm.c osm.h 
	$(CC) $(CFLAGS) osm.c

clean:
	rm -rf $(OSMLIB) $(TARNAME)

main: osm.o
	$(CC) $(CFLAGS) -o main osm.o

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

run: main
	./main 1000

.PHONY: tar clean
