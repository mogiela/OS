#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/fs.h>
#include <iostream>


#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h> // REMOVE ON SUBMIT
#include <stdlib.h>
#include <limits.h>

#include "osm.h"

#define DEFAULT_ITER 1000
#define MICRO_TO_NANO 1000
#define SEC_TO_NANO 1000000000
#define CMD_PER_ITER 10
#define ERR -1
#define DISK_SEC_SIZE 512
#define FILE_FOR_DISK_TEST "/tmp/LIRANE_BITTON_HAMELECH.txt"

typedef unsigned int ulint;
typedef struct timeval timeval;

char* machineName = 0;

double checkError(double m_Time){
	if(!m_Time){
		return ERR;
	}
	return m_Time;
}


double time_calc_diff(timeval* tv0, timeval* tv1, unsigned int iterations){
	double t0 = (*tv0).tv_sec*SEC_TO_NANO + (*tv0).tv_usec*MICRO_TO_NANO;
	double t1 = (*tv1).tv_sec*SEC_TO_NANO + (*tv1).tv_usec*MICRO_TO_NANO;
	return (t1 - t0)/(double)(iterations*CMD_PER_ITER);
}


/*
// REMOVE THE NEXT 2 FUNCTIONS IF THEY ARE NOT USED UNTIL END OF EX
*/

/* Initialization function that the user must call
 * before running any other library function.
 * The function may, for example, allocate memory or
 * create/open files.
 * Returns 0 uppon success and -1 on failure
 */
int osm_init(){
	//adding room for terminating null byte
	machineName = (char*)malloc(sizeof(char) * HOST_NAME_MAX+1);
	if(machineName == 0) return ERR;
	return 0;
}


/* finalizer function that the user must call
 * after running any other library function.
 * The function may, for example, free memory or
 * close/delete files.
 * Returns 0 uppon success and -1 on failure
 */
int osm_finalizer(){
	free(machineName);
	return 0;
}



/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
*/
double osm_operation_time(unsigned int iterations) {
  
	if (iterations == 0) iterations = DEFAULT_ITER;
	//calcualte iterations
	iterations =   ceilf((float)iterations / (float)CMD_PER_ITER);  
	timeval tv0;
	timeval tv1;
	if (gettimeofday(&tv0, 0) == ERR) return ERR;
	for (ulint i = 0; i < iterations; ++i){
	  
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
	  (void) (2+1);
		
	}
	if (gettimeofday(&tv1, 0) == ERR) return ERR;
	return checkError(time_calc_diff(&tv0, &tv1, iterations));
}

void empty_func(void){};

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success, 
   and -1 upon failure.
*/
double osm_function_time(unsigned int iterations)
{
	if (iterations == 0) iterations = DEFAULT_ITER;
	//calcualte iterations
	iterations =   ceilf((float)iterations / (float)CMD_PER_ITER);  
	timeval tv0;
	timeval tv1;
	if (gettimeofday(&tv0, 0) == ERR) return ERR;
	for (ulint i = 0; i < iterations; ++i){
		empty_func();
		empty_func();
		empty_func();
		empty_func();
		empty_func();
		empty_func();
		empty_func();
		empty_func();
		empty_func();
		empty_func();
	}
	if (gettimeofday(&tv1, 0) == ERR) return ERR;
	return checkError(time_calc_diff(&tv0, &tv1, iterations));
}




/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success, 
   and -1 upon failure.
*/
double osm_syscall_time(unsigned int iterations){
	if (iterations == 0) iterations = DEFAULT_ITER;
	//calcualte iterations
	iterations =   ceilf((float)iterations / (float)CMD_PER_ITER);
	timeval tv0;
	timeval tv1;
	if (gettimeofday(&tv0, 0) == ERR) return ERR;
	for (ulint i = 0; i < iterations; ++i){
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
	}
	if (gettimeofday(&tv1, 0) == ERR) return ERR;
	return checkError(time_calc_diff(&tv0, &tv1, iterations));
}


/* Time measurement function for accessing the disk.
   returns time in nano-seconds upon success, 
   and -1 upon failure.
*/
double osm_disk_time(unsigned int iterations) {
	timeval tv0;	//time structs for time calcs...
	timeval tv1;
	if (iterations == 0) iterations = DEFAULT_ITER;
	//calcualte iterations
	iterations =   ceilf((float)iterations / (float)CMD_PER_ITER);

	/* open a file and write something to it, if fails disqualify test */
	char image[] = {'A','L','L',' ','H','A','I','L',' ','T','Z','A','R',' ',
					'L','I','R','A','N','E',' ','B','I','T','T','O','N'};
	int fd = open(FILE_FOR_DISK_TEST, O_SYNC|O_DIRECT|O_RDWR|O_CREAT|O_TRUNC, 0777);

	/* allocate memory to read, if fails disqualify test */
	void* aligned_mem = 0;
	posix_memalign(&aligned_mem, DISK_SEC_SIZE, DISK_SEC_SIZE);
	memcpy(aligned_mem, image, sizeof(image));

	//write test
	if (write(fd, aligned_mem, DISK_SEC_SIZE) < 0) return ERR;

	if (gettimeofday(&tv0, 0) == ERR) return ERR;
	for (ulint i = 0; i < iterations; ++i){
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
		write(fd, aligned_mem, DISK_SEC_SIZE);
	}

	if (gettimeofday(&tv1, 0) == ERR) return ERR;
	close(fd);
	unlink(FILE_FOR_DISK_TEST);
	free(aligned_mem);
	return checkError(time_calc_diff(&tv0, &tv1, iterations));
}


timeMeasurmentStructure measureTimes (unsigned int operation_iterations,
									  unsigned int function_iterations,
									  unsigned int syscall_iterations,
									  unsigned int disk_iterations){
	
	double o_time = osm_operation_time(operation_iterations);
	double f_time = osm_function_time(function_iterations);
	double s_time = osm_syscall_time(syscall_iterations);
	double d_time = osm_disk_time(disk_iterations);

	//check if any of them are -1 and if they are then disqualify the result
	double fi_ratio = ( o_time<0 || f_time<0 ) ? ERR : f_time / o_time;
	double si_ratio = ( o_time<0 || s_time<0 ) ? ERR : s_time / o_time;
	double di_ratio = ( o_time<0 || d_time<0 ) ? ERR : d_time / o_time;
	
	
	gethostname(machineName, HOST_NAME_MAX+1);
	timeMeasurmentStructure tm = {machineName,
								  o_time, f_time, s_time, d_time,
								  fi_ratio, si_ratio, di_ratio};
	return tm;
}

