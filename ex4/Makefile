CC=g++
DBG=-D_DEBUG
CFLAGS=-std=c++11 -Wall  -g $(DBG)
VAL_FLAGS=
EXN=4
EX_SRC=


CPP_FILES=CachingFileSystem.cpp block.cpp cache.cpp log_util.c
OTHER_FILES=Makefile README late_sub_aproval.png
FILES=block.o cache.o CachingFileSystem.cpp
OUTPUT=CachingFileSystem

RUN=run
OUT=out

CLION_BIN=../bin
TEST_DIR=tests
TEST_SRC=$(TEST_DIR)/src
TEST_BIN=$(TEST_DIR)/bin
TEST_OUT=$(TEST_DIR)/output


all: cachingFS

cachingFS: cache block

	g++ ${CFLAGS} $(FILES) `pkg-config fuse --cflags --libs` -o $(OUTPUT)


block: block.cpp
	${CC} ${CFLAGS} block.cpp -c -o block.o

cache: cache.cpp
	${CC} ${CFLAGS} cache.cpp -c -o cache.o

tar:
	tar cvf ex$(EXN).tar $(CPP_FILES) $(OTHER_FILES)

clean:
	rm -rf *.o CachingFileSystem ex4.tar










