#include "block.cpp"
#include <vector>
#include <list>
#include <iostream>
#include <stdio.h>
#include <regex>


#ifndef cache_cpp
#define cache_cpp
#define CMAX 50
#define ERR -1

using std::list;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::regex;

class Cache {
public:

	Block** _blocks;
	vector<Block**> _blkStack;
	list<Block**>* _refCountChain;
	
	size_t _blkSize;
	size_t _numBlocks;

	size_t _midStart;
	size_t _oldStart;
	
	

	Cache(size_t blkSize, int numBlocks, double fNew, double fOld):
			_blkSize(blkSize),
			_numBlocks((size_t)numBlocks),
			_midStart((size_t) (fNew*numBlocks)),
			_oldStart(numBlocks-((size_t) (fOld*numBlocks)-1)) {


		_blocks = new Block*[_numBlocks];
		for (size_t i = 0 ; i < _numBlocks ; ++i){
			_blocks[i] = new Block(_blkSize);
		}
	  	_blkStack.reserve(_numBlocks);
		_refCountChain = new list<Block**>[CMAX];
	}

	~Cache(){
		for (size_t i = 0 ; i < _numBlocks ; ++i){
			delete _blocks[i];
		}
		delete[] _blocks;
		delete[] _refCountChain;
	}

	//finds if a block already exist in the cache by blkNum and path
	int inCache(string path, size_t blkNum){
		for (int i = 0; i < (int)_blkStack.size(); ++i){
			if ((*_blkStack[i])->_path == path){
				if ((*_blkStack[i])->_blkNum == blkNum){
					return i;
				}
			}
		}
		return ERR;
	}


	/*
	 * 	inserts a block to blkStack and to the relevant refCountChain
	 * 	also change the isNew and isOld and take care of midStart,oldStart
	 */
	int insertToDSs(Block** blkPtr){

		//insert to blkStack
		_blkStack.insert(_blkStack.begin(),blkPtr);
		(*_blkStack[0])->_isNew = true;
		(*_blkStack[0])->_isOld = false;

		//update mid pointer status
		if(_midStart < _blkStack.size()){
			(*_blkStack[_midStart])->_isNew = false;
		}
		//update old pointer status
		if(_oldStart+1 < _blkStack.size()){
			(*_blkStack[_oldStart+1])->_isOld = true;
		}
		//insert to refCountChain in the relevant refCount
		if ((*blkPtr)->_refCount < CMAX){
			_refCountChain[(*blkPtr)->_refCount-1].push_front(blkPtr);
		}

		return 0;
	}

	//finds blocks and if found removes from blkStack and refCountChain
	Block** findRemoveFromDSs(string path, size_t blkNum){

		int pos = 0;

		if ( (pos = inCache(path, blkNum)) == ERR ){
			return nullptr;
		}
		Block** found = _blkStack[pos];

		_blkStack.erase(_blkStack.begin() + pos);

		//if not then its not in refCountChain
		if ((*found)->_refCount < CMAX){
			_refCountChain[(*found)->_refCount-1].remove(found);
		}
		


		return found;
	}

	//the fbr algorithm
	Block** getVictim(){
		//get victim from refCountChain
		for (int i = 0; i < CMAX; ++i) {
			if (_refCountChain[i].size() > 0) {
				if ((*(_refCountChain[i].back()))->_isOld) {
					return _refCountChain[i].back();
				}
			}
		}
		return _blkStack[_numBlocks-1];
	}

	//reads from the block into the given buffer
	static ssize_t readFromBlock(Block* block, void* buf, size_t offset,
								 size_t nbytes) {

        if (block->_dataEnd < offset ){
            return 0;
        }

		size_t read_amount = block->_dataEnd - offset > nbytes ? nbytes :
							 block->_dataEnd - offset;

        memcpy(buf, (char*) block->_algnData +  offset, read_amount);

		return read_amount;
	}


	/*
		the read func.
	 	has three parts : 	cache hit,
	 						cache miss and not a full cache or
	 						cache miss and a full cache.
		retruns the num of read bytes.
	
	*/
	ssize_t read(int fd, void* buf, size_t nbytes, off_t offset, string path){
		//get blknum and offset
		size_t blkNum = (offset/_blkSize)+1;
		size_t blkOffset = offset%_blkSize;
		ssize_t retval = 0;

		//checking if the block already exist in the cache
		Block** target = findRemoveFromDSs(path, blkNum);

		//if cache hit
		if ( target != nullptr ){

			//checking if its in the new zone
			if(!((*target)->_isNew)){
				++(*target)->_refCount; }
			//putting it in the head of the blkStack
			insertToDSs(target);
			return readFromBlock(*target, buf, blkOffset, nbytes);

		}
		//cache miss
		//cache is not full
		if(_blkStack.size() != _numBlocks){
			//find first empty block
			for (size_t i = 0; i < _numBlocks; ++i ){
				if (_blocks[i]->_dataEnd == 0){

					// insert the new block instead
					_blocks[i]->effes();
					_blocks[i]->initVar(_blkSize, path, fd, blkNum);
					if ( (retval = _blocks[i]->readIntoBlock()) < 0){
						//reading failed, zero out block
						_blocks[i]->effes();
						return retval;
					}

					insertToDSs(_blocks + i);

					return readFromBlock(*_blkStack[0], buf, blkOffset, nbytes);
				}
			}
		}
		//cache full:

		//finding which block to overwrite
		Block** victim = getVictim();

		//removing it from the dasts
		findRemoveFromDSs((*victim)->_path, (*victim)->_blkNum);

		//reinit
		(*victim)->initVar(_blkSize, path, fd, blkNum);

		if( (retval = (*victim)->readIntoBlock()) < 0 ){
			//reading failed, zero out block
			(*victim)->effes();
			return retval;
		}

		insertToDSs(victim);

		return readFromBlock(*victim, buf, blkOffset, nbytes);
	}

	//renaming a folder or a file in the cache
	void renamePath(string oldPath, string newPath){
		//only the first time we find it will be changed
		std::regex_constants::match_flag_type fOnly =
				std::regex_constants::format_first_only;

		//checking all the blocks in the cache to change the relevant path
		for (int i = 0; i < (int)_blkStack.size(); ++i){

			//finding all the oldPath in the (*_blkStack[i])->_path
			// and replacing only the first time with newPath
			(*_blkStack[i])->_path = std::regex_replace((*_blkStack[i])->_path,
			std::regex(oldPath.c_str()), newPath, fOnly);

		}
	}

	//writing the state of the cache to the log
	void writeCacheState(FILE* log_file){

		for (int i = 0; i < (int)_blkStack.size(); ++i){
			string path = (*_blkStack[i])->_path;
			size_t blkNum = (*_blkStack[i])->_blkNum;
			unsigned int counter = (*_blkStack[i])->_refCount;
			fprintf(log_file,"%s %lu %u\n", path.c_str(), blkNum, counter);
		}
	}
};

#endif // cache_cpp