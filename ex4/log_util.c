
#ifndef LOG_UTIL
#define LOG_UTIL

#include <cstdlib>
#include <stdio.h>
#include <cstdarg>
#include <ctime>



FILE* log_open(char* logfile_name)
{
    FILE* logfile;
    
    // very first thing, open up the logfile and mark that we got in
    // here.  If we can't open the logfile, we're dead.
    logfile = fopen(logfile_name, "a");
    if (logfile == NULL) {
    fprintf(stderr, "System Error: realpath failed\n");
	exit(EXIT_FAILURE);
    }
    
    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);

    return logfile;
}

void log_msg(FILE* log, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vfprintf(log, format, ap);
}



      
// make a system call, checking (and reporting) return status and
// possibly logging error
int func_handle(FILE* log, const char *func, int retstat, int min_ret)
{
	time_t epoch_time = time(NULL);
	log_msg(log, "%d %s\n", epoch_time, func);

	if (retstat < min_ret) {
	retstat = -errno;
    }

    return retstat;
}


#endif
