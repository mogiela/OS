#include <stdlib.h>
#include "uthreads.h"
#include <sys/time.h>	//itimer
#include <cstddef>		//NULL definition
#include <iostream>		//std::cout std::cin std::endl
#include <list>      	// std::list
#include <signal.h>
#include "uthread.cpp"

#include <cmath>


using std::cerr;
using std::cout;
using std::cin;
using std::endl;
using std::list;


#define MICRO_TO_SEC(val) val/1e6 /*Translate microseconds to seconds*/
#define SEC_TO_MICRO(val) val*1e6 /*translate seconds to microseconds*/
#define ERR -1
//#define SYSCALLCHECK(call) do while false);
#define SYSCALLCHECK(call) if(call < 0){ cerr << "error in: "<<"call" \
<< endl; exit(1);}


///////////////////////////////////////////////////////////////
//Printing funcs and macros used for Debug//
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(str)  std::cout << str; cout.flush();

#define DEBUG_LIST(list) debug_print_list(list);

#else
#define DEBUG_PRINT(str) //do { } while ( false )
#define DEBUG_LIST(list)

#endif

///////////// End debug functions/////////////////////
////////////////////////////////////////////////////

//threads containers
uthread* threads[MAX_THREAD_NUM];
uthread* pointer_to_free = nullptr;
list<uthread*> ready;

//handler
struct sigaction sa;

//signal sets for future use
sigset_t new_set;


int tot_quants = 1;
int counter_sleep = 0;
itimerval quanta_timer;
itimerval zeros;
int runningID = 0;



int get_first_free_id(){
	int i = 0;
	while((threads[i] != nullptr) && i < MAX_THREAD_NUM) {++i;}
	if (i == MAX_THREAD_NUM){
		cerr << "thread library error: you reached the max number of threads"
		<< endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;}

	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return i;
}



void sleepless(){
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL))

	if(counter_sleep != 0){
		for (int i = 0; i < MAX_THREAD_NUM; ++i) {
			if (threads[i] != nullptr) {
				if (threads[i]->_stat == SLEEP) {
					threads[i]->_s_count--;
					if (threads[i]->_s_count == 0) {
						threads[i]->_stat = READY;
						ready.push_back(threads[i]);
						counter_sleep--;
					}
				}
			}
		}
	}

	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL))
}



void remove_pending_sig() {

	int waitDummy = 1;//solely for getting rid of the pending signals.
	sigset_t waiting;

	SYSCALLCHECK(sigemptyset(&waiting));
	SYSCALLCHECK(sigpending(&waiting));

	if (sigismember(&waiting, SIGVTALRM) > 0) {

		if (sigwait(&waiting, &waitDummy) > 0) {
			cerr << "system error: sigwait failure" << endl;
			exit(1);
		}
	}

}

void switch_thrd(int sigNum) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL))
	tot_quants++;
	if(pointer_to_free != nullptr){
		delete pointer_to_free;
		pointer_to_free = nullptr;
	}
	sleepless();

	//if there is only one thread
	if (ready.size() == 0) {
		threads[runningID]->_q_count++;
		remove_pending_sig();
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL))
		return;

	}

	//set of the old running
	int ret = sigsetjmp(threads[runningID]->_env, 1);

	//if we coming back from a jump
	if (ret != 0) {
		remove_pending_sig();
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL))
		return;
	}

	//old running
	threads[runningID]->_stat = READY;
	ready.push_back(threads[runningID]);

	//new running
	runningID = ready.front()->_tid;
	threads[runningID]->_stat = RUNIN;
	threads[runningID]->_q_count++;
	ready.pop_front();


	remove_pending_sig();


	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	SYSCALLCHECK(setitimer(ITIMER_VIRTUAL, &quanta_timer, NULL));

	siglongjmp(threads[runningID]->_env, 1);


	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
}




void hand_thrd() {

	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL))
	tot_quants++;

	sleepless();

	//new running
	runningID = ready.front()->_tid;
	threads[runningID]->_stat = RUNIN;
	threads[runningID]->_q_count++;
	ready.pop_front();

	remove_pending_sig();

	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	SYSCALLCHECK(setitimer(ITIMER_VIRTUAL, &quanta_timer, NULL));

	siglongjmp(threads[runningID]->_env, 1);
}



/** initialize timer*/
void setTimerStruct(int quantum_usecs){
	double sec_quantum = MICRO_TO_SEC(quantum_usecs);
	quanta_timer.it_interval.tv_sec = floor(sec_quantum);
	quanta_timer.it_interval.tv_usec = SEC_TO_MICRO((sec_quantum -
			floor(sec_quantum)));
	quanta_timer.it_value.tv_sec = quanta_timer.it_interval.tv_sec;
	quanta_timer.it_value.tv_usec = quanta_timer.it_interval.tv_usec;
	zeros.it_interval.tv_sec = 0;
	zeros.it_interval.tv_usec = 0;
	zeros.it_value.tv_sec = 0;
	zeros.it_value.tv_usec = 0;
}

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
	sigemptyset(&new_set);
	sigaddset(&new_set, SIGVTALRM);

	if (quantum_usecs <= 0) {
		cerr << "thread library error: quantum_usecs must be positive" << endl;
		return ERR;	}

	sa.sa_handler = switch_thrd;

	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
		return ERR;
	}

	//initializing the timer
	setTimerStruct(quantum_usecs);

	for(int i = 0; i < MAX_THREAD_NUM; ++i){
		threads[i] = nullptr;
	}

	// the main thread pc will be loaded when it switches
	uthread_spawn(nullptr);

	threads[0]->_stat = RUNIN;
	threads[0]->_q_count++;
	runningID = 0;

	// the main thread will go into the ready list when it switches, not now
	ready.pop_front();

	SYSCALLCHECK(setitimer(ITIMER_VIRTUAL,&zeros, NULL));
	SYSCALLCHECK(setitimer(ITIMER_VIRTUAL,&quanta_timer, NULL));
	return 0;
}


/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void)) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL));

	int tid;
	tid = get_first_free_id();

	// in case there's no room for more threads
	if (tid < 0) {
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR; }

	//creating new thread.
	threads[tid] = new uthread(tid);
	sigsetjmp(threads[runningID]->_env,1);


	//translation
	address_t  stackArray = (address_t)(threads[tid]->_sp + STACK_SIZE
										- sizeof(address_t));
	address_t pc = (address_t) f;

	(threads[tid]->_env->__jmpbuf)[JB_SP] = translate_address(stackArray);
	(threads[tid]->_env->__jmpbuf)[JB_PC] = translate_address(pc);


	ready.push_back(threads[tid]);
	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return tid;
}



/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread		running->_q_count++;
 should be released. If no thread with ID tid
 * exists it is considered as an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL));

	if (tid == 0) {
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		exit(0);
	}

	if (tid < 0) {
		cerr << "thread library error: the thread id is invalid" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}


	if (threads[tid] == nullptr) {
		cerr << "thread library error: No such thread" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	int stat = threads[tid]->_stat;

	switch (stat) {
		case SLEEP:
			cerr << "thread library error: can't terminate a sleeping thread"
			<< endl;
			SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
			return ERR;

		case READY:
			ready.remove(threads[tid]);
			break;

		case RUNIN:
			//free tid
			pointer_to_free = threads[tid];
			threads[tid] = nullptr;
			hand_thrd();           				 //scheduling decision
			break;
	}

	//free tid
	delete threads[tid];
	threads[tid] = nullptr;
	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return 0;

}


/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED or SLEEPING states has no
 * effect and is not considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/

int uthread_block(int tid) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL));
	if (tid == 0) {
		cerr << "thread library error: can't block main thread" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	if (tid < 0) {
		cerr << "thread library error: the thread id is invalid" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	if (threads[tid] == nullptr) {
		cerr << "thread library error: No such thread" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	char stat = threads[tid]->_stat;
	threads[tid]->_stat = BLOCK;

	if (stat == READY) {
		ready.remove(threads[tid]);
	}

	if (stat == RUNIN) {
		int blocking_beauty = sigsetjmp(threads[tid]->_env, 1);
		if (blocking_beauty != 0)
		{
			SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
			return 0;
		}
		hand_thrd();            			//scheduling decision
	}

	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return 0;
}


/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in the RUNNING, READY or SLEEPING
 * state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL));

	if (tid < 0) {
		cerr << "thread library error: the thread id is invalid" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	if (threads[tid] == nullptr) {
		cerr << "thread library error: No such thread" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	if(threads[tid]->_stat == BLOCK) {
		threads[tid]->_stat = READY;
		ready.push_back(threads[tid]);
	}

	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return 0;
}


/*
 * Description: This function puts the RUNNING thread to sleep for a period
 * of num_quantums (not including the current quantum) after which it is moved
 * to the READY state. num_quantums must be a positive number. It is an error
 * to try to put the main thread (tid==0) to sleep. Immediately after a thread
 * transitions to the SLEEPING state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL));

	if (runningID == 0) {
		cerr << "thread library error: can't put the main thread to sleep"
		<< endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR; }
	if (num_quantums <= 0) {
		cerr << "thread library error: Time to sleep must be a positive number"
		<< endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR; }

	threads[runningID]->_s_count = num_quantums + 1;
	threads[runningID]->_stat = SLEEP;
	counter_sleep++;

	int sleeping_beauty = sigsetjmp(threads[runningID]->_env, 1);
	if (sleeping_beauty != 0)
	{
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return 0;
	}

	hand_thrd();            				//scheduling decision
	return 0;

}


/*
 * Description: This function returns the number of quantums until the thread
 * with id tid wakes up including the current quantum. If no thread with ID
 * tid exists it is considered as an error. If the thread is not sleeping,
 * the function should return 0.
 * Return value: Number of quantums (including current quantum) until wakeup.
*/
int uthread_get_time_until_wakeup(int tid) {
	SYSCALLCHECK(sigprocmask(SIG_BLOCK, &new_set, NULL));

	if (threads[tid] == nullptr || tid < 0) {
		cerr << "thread library error: No such thread" << endl;
		SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
		return ERR;
	}

	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return threads[tid]->_s_count;
}


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid() {
	SYSCALLCHECK(sigprocmask(SIG_UNBLOCK, &new_set, NULL));
	return runningID;
}


/*
 * Description: This function returns the total number of quantums that were
 * started since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums() {
	return tot_quants;
}


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered as an error.
 * Return value: On success, return the number of quantums of the thread with
 * ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
	if (tid < 0) {
		cerr << "thread library error: the thread id is invalid" << endl;
		return ERR;
	}

	if (threads[tid] == nullptr) {
		cerr << "thread library error: No such thread" << endl;
		return ERR;
	}

	return threads[tid]->_q_count;
}






