/*
 * CachingFileSystem.cpp
 *
 *  Author: Netanel Zakay, HUJI, 67808  (Operating Systems 2015-2016).
 */

#define FUSE_USE_VERSION 26

#include "CachingFileSystem.h"
#include "log_util.c"
#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include "cache.cpp"


using std::cout;
using std::cerr;

using namespace std;

struct fuse_operations caching_oper;

static Cache* cache;
static char rootdir[PATH_MAX] = "";
static FILE* log_file;




//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void cache_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
									// break here

}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int caching_getattr(const char *path, struct stat *statbuf){
	char fpath[PATH_MAX];
	cache_fullpath(fpath, path);

	int retstat = func_handle(log_file, __FUNCTION__, lstat(fpath, statbuf), 0);
	return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the 
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int caching_fgetattr(const char *path, struct stat *statbuf, 
					struct fuse_file_info *fi){
	int retstat = func_handle(log_file, __FUNCTION__,
							  fstat(fi->fh, statbuf), 0);
    return retstat;
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int caching_access(const char *path, int mask)
{
	char fpath[PATH_MAX];
	cache_fullpath(fpath, path);

	int retstat = func_handle(log_file, __FUNCTION__, access(fpath, mask), 0);
    return retstat;
}


/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * initialize an arbitrary filehandle (fh) in the fuse_file_info 
 * structure, which will be passed to all file operations.
 *
 * pay attention that the max allowed path is PATH_MAX (in limits.h).
 * if the path is longer, return error.
 *
 * Changed in version 2.2
 */
int caching_open(const char *path, struct fuse_file_info *fi){

	//the user is not allowed to open the log
	if(strcmp(path,"/.filesystem.log") == 0 ){
		return -ENOENT;
	}

	char fpath[PATH_MAX];
	cache_fullpath(fpath, path);

	fi->direct_io = 1;

	//checking that the file is opened only in read
	if((fi->flags & 3)!= O_RDONLY) {
		return -EACCES;
	}
	//using O_RDONLY |O_DIRECT |O_SYNC
	int fd = func_handle(log_file, __FUNCTION__,open(fpath,O_RDONLY |
														   O_DIRECT |
														   O_SYNC),0);
	//if the open does not work
	if (fd < 0){
		return -errno;
	}

	//the place we store the fd in fuse
	fi->fh = fd;

	return 0;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error. For example, if you receive size=100, offest=0,
 * but the size of the file is 10, you will init only the first 
   ten bytes in the buff and return the number 10.
   
   In order to read a file from the disk, 
   we strongly advise you to use "pread" rather than "read".
   Pay attention, in pread the offset is valid as long it is 
   a multipication of the block size.
   More specifically, pread returns 0 for negative offset 
   and an offset after the end of the file
   (as long as the the rest of the requirements are fulfiiled).
   You are suppose to preserve this behavior also in your implementation.

 * Changed in version 2.2
 */
int caching_read(const char *path, char *buf, size_t size,
				 off_t offset, struct fuse_file_info *fi){

	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time,__FUNCTION__);


	size_t nBytesLeft = size;
	int nBytesRead = 0;
	//while there is more to read keep on reading
	while (nBytesLeft > 0) {
		//read
		// buf + nBytesRead so we will not overwrite what we just write
		// nBytesLeft so we will keep reading the size that we need
		// offset + nBytesRead so we will read with the right offset
		ssize_t retstat = cache->read(fi->fh, buf + nBytesRead,
									  nBytesLeft, offset + nBytesRead, path);

		//the read failed
		if (retstat < 0 ) 		{
			return -errno; } // return error
		//the read finished
		else if (retstat == 0)	{
			// end loop return nBytesRead
			nBytesLeft = 0;

		}

		else{
			// decrease nBytesLeft ,increase nBytesRead repeat
			nBytesLeft -= retstat;
			nBytesRead += retstat;

		}
	}

	return nBytesRead;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int caching_flush(const char *path, struct fuse_file_info *fi)
{
	//we don't write so we don't flush
	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time, __FUNCTION__);
	
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int caching_release(const char *path, struct fuse_file_info *fi){
	return func_handle(log_file, __FUNCTION__,close(fi->fh),0);
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int caching_opendir(const char *path, struct fuse_file_info *fi){
	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time,__FUNCTION__);
	
	DIR *direc;
	int retstat = 0;
	char fpath[PATH_MAX];
	
	cache_fullpath(fpath, path);

	//open a dir
	direc = opendir(fpath);
	if(direc == NULL){
		retstat = -errno;
	}

	//pointer to the dir
	fi->fh = (intptr_t) direc;
	
	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * Introduced in version 2.3
 */
int caching_readdir(const char *path, void *buf, 
					fuse_fill_dir_t filler, 
					off_t offset, struct fuse_file_info *fi){

	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time,__FUNCTION__);

	//pointer to the dir
	DIR* direc = (DIR *) (intptr_t) fi->fh;
	struct dirent* ent = readdir(direc);

	//check that the call went well
	if (ent == NULL){
		return -errno;
	}

	//check that the buffer is not full
	if (filler(buf, ent->d_name, NULL, 0) != 0) {
		return -ENOMEM;
	}

	//continue filling the buffer until the buffer is
	//full or the whole directory has been read
	while((ent = readdir(direc)) != NULL){
		if (filler(buf, ent->d_name, NULL, 0) != 0) {
			return -ENOMEM;
		}
	}
	
	return 0;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int caching_releasedir(const char *path, struct fuse_file_info *fi){
	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time,__FUNCTION__);

	//close the dir
	closedir((DIR *) (uintptr_t) fi->fh);
	return 0;
}


/** Rename a file */
int caching_rename(const char *path, const char *newpath){

	if(strcmp(path,"/.filesystem.log") == 0 ){
		return -ENOENT;
	}

	char fpath[PATH_MAX];
	cache_fullpath(fpath, path);

	char newFpath[PATH_MAX];
	cache_fullpath(newFpath, newpath);

	//calling the syscall that rename the real dir
	int retstat = func_handle(log_file, __FUNCTION__,rename(fpath,newFpath),0);

	//if it fails don't update the cache
	if (retstat < 0){
		return retstat;
	}

	//update the cache according to the new name
	cache->renamePath((string) path,(string) newpath);

	return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 
If a failure occurs in this function, do nothing (absorb the failure 
and don't report it). 
For your task, the function needs to return NULL always 
(if you do something else, be sure to use the fuse_context correctly).
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *caching_init(struct fuse_conn_info *conn){
	
	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time,__FUNCTION__);
	
	return NULL;
}


/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
  
If a failure occurs in this function, do nothing 
(absorb the failure and don't report it). 
 
 * Introduced in version 2.3
 */
void caching_destroy(void *userdata){
	time_t epoch_time = time(nullptr);
	log_msg(log_file, "%d %s\n", epoch_time,__FUNCTION__);

	//calls the destructor of the cache and blocks
	delete cache;

	//closes the log file
	fclose(log_file);
}


/**
 * Ioctl from the FUSE sepc:
 * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
 * 64bit environment.  The size and direction of data is
 * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
 * data will be NULL, for _IOC_WRITE data is out area, for
 * _IOC_READ in area and if both are set in/out area.  In all
 * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
 *
 * However, in our case, this function only needs to print 
 * cache table to the log file .
 * 
 * Introduced in version 2.8
 */
int caching_ioctl (const char *, int cmd, void *arg,
		struct fuse_file_info *, unsigned int flags, void *data){

	//writes the status of the cache to the log
	cache->writeCacheState(log_file);
	return 0;
}


// Initialise the operations. 
// You are not supposed to change this function.
void init_caching_oper()
{

	caching_oper.getattr = caching_getattr;
	caching_oper.access = caching_access;
	caching_oper.open = caching_open;
	caching_oper.read = caching_read;
	caching_oper.flush = caching_flush;
	caching_oper.release = caching_release;
	caching_oper.opendir = caching_opendir;
	caching_oper.readdir = caching_readdir;
	caching_oper.releasedir = caching_releasedir;
	caching_oper.rename = caching_rename;
	caching_oper.init = caching_init;
	caching_oper.destroy = caching_destroy;
	caching_oper.ioctl = caching_ioctl;
	caching_oper.fgetattr = caching_fgetattr;


	caching_oper.readlink = NULL;
	caching_oper.getdir = NULL;
	caching_oper.mknod = NULL;
	caching_oper.mkdir = NULL;
	caching_oper.unlink = NULL;
	caching_oper.rmdir = NULL;
	caching_oper.symlink = NULL;
	caching_oper.link = NULL;
	caching_oper.chmod = NULL;
	caching_oper.chown = NULL;
	caching_oper.truncate = NULL;
	caching_oper.utime = NULL;
	caching_oper.write = NULL;
	caching_oper.statfs = NULL;
	caching_oper.fsync = NULL;
	caching_oper.setxattr = NULL;
	caching_oper.getxattr = NULL;
	caching_oper.listxattr = NULL;
	caching_oper.removexattr = NULL;
	caching_oper.fsyncdir = NULL;
	caching_oper.create = NULL;
	caching_oper.ftruncate = NULL;
}

//basic main. You need to complete it.
int main(int argc, char* argv[]){

	//cheacks that we got 5 args
	if(argc < 5){
		cout << "Usage: CachingFileSystem rootdir mountdir " <<
						"numberOfBlocks fOld fNew\n";
		return ERR;
	}
	//will be used to check if there was a problem and if there
	// was then to print
	bool print_usage = false;
	
	//all of this is for checking if the mount and root are valid dirs.
	struct stat mount_stat;
	struct stat root_stat;
	stat(argv[2], &mount_stat);	
	stat(argv[1], &root_stat);
	if( !S_ISDIR(mount_stat.st_mode) || !S_ISDIR(root_stat.st_mode) ){
		print_usage = true;
	}

//	get block size
	struct stat fi;
	stat("/tmp", &fi);
	int blkSize = fi.st_blksize;

	//converts the string of numBlocks to numbers and checks validity
	int numBlocks = atoi(argv[3]);
	if(numBlocks <= 0){ print_usage = true; }

	//converts the string of fNew & fOld to numbers and checks validity
	double fNew = atof(argv[5]);
	double fOld = atof(argv[4]);
	if(fNew <= 0 || fNew >= 1 || fOld <= 0 || fOld >= 1 || fOld + fNew > 1){
		print_usage = true;
	}

	//prints if there is a problem
	if(print_usage){
		cout << "Usage: CachingFileSystem rootdir mountdir " <<
						"numberOfBlocks fOld fNew\n";
		return ERR;
	}

	//makes a realpath out of rootdir if fails says so and returns ERR
	if(realpath(argv[1], rootdir) == NULL){
		cerr << "System Error: realpath failed\n";
		return ERR;		
	}

    //creates the log name and path
	char log_name[PATH_MAX] = "";
	strcpy(log_name, rootdir);
	strcat(log_name, "/.filesystem.log");

	//creates a cache
	cache = new Cache(blkSize, numBlocks, fNew, fOld);

	//init args for the fuse main
	init_caching_oper();
	argv[1] = argv[2];
	for (int i = 2; i < (argc - 1); i++){
		argv[i] = NULL;
	}
	argv[2] = (char*) "-s";
	argc = 3;

//	if you want to see prints
//	argv[3] = (char*) "-f";
//	argc = 4;

	//creates the log file
	log_file = log_open(log_name);

	//FUSE
	int fuse_stat = fuse_main(argc, argv, &caching_oper, NULL);

	return fuse_stat;
	
}



