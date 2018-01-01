#include <string>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <iostream>


#ifndef block_cpp
#define block_cpp
#define ERR -1
using std::string;


class Block {
public:
	void* _algnData;
	size_t _blkSize;
	string _path;
	int _fd;
	size_t _blkNum;
	
	size_t _dataEnd;
	
	bool _isNew = false;
	bool _isOld = false;
	
	unsigned int _refCount;

	//default constructor
	Block(size_t blkSize = 0, string path = "", int fd = 0, size_t blkNum = 0):
			_blkSize(blkSize),
			_path(path),
			_fd(fd),
			_blkNum(blkNum) {


		//assign data and first vars
		posix_memalign(&_algnData, blkSize, blkSize);
		_refCount = 0;
		_dataEnd = 0;
		memset(_algnData,0, blkSize);
	}

	ssize_t readIntoBlock(){
		//read the relevant block from the file:

		ssize_t retval = pread(_fd, _algnData, _blkSize, _blkSize*(_blkNum-1));
	    _dataEnd = (size_t) retval;

		//if the reading failed
		if(retval == ERR){
			_dataEnd = 0;
		}

		++_refCount;
		return retval;
	}
	~Block() {
		free(_algnData);
	}

	//in case we want all the vars to zero out
	void effes(){
		_dataEnd = 0;
		_blkNum = 0;
		_isNew = false;
		_isOld = false;
		_fd = 0;
		_blkSize = 0;
		_refCount = 0;
		_path = "";
	}

	//to init the vars before reading to a block
	void initVar(size_t blkSize, string path, int fd, size_t blkNum){

		_blkSize = blkSize;
		_path = path;
		_fd = fd;
		_blkNum = blkNum;
		_isNew = true;
		_isOld = false;

	}
	
};

#endif