CC = g++
CFLAGS = -Wall -g -std=c++11
LIBFLAGS = ar -rcs

TAR = tar
TARFLAGS = -cvf
TARNAME = ex2.tar
TARSRCS =  Makefile README uthreads.cpp uthread.cpp
TESTS_FILE = ./tests/test.cpp


libthreads.a: uthreads.o
	$(LIBFLAGS) libthreads.a $<

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

uthreads.o: uthreads.cpp uthreads.h
	$(CC) $(CFLAGS) -c $<

.cpp.o:
	$(CC) $(CFLAGS) -c $<


clean:
	rm -fr *.o libthreads.a


.PHONY: tar clean


