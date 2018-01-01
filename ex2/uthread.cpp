#include <setjmp.h>		//sigsetjmp siglongjmp
#include <iostream>
#include <new>
#include <cstdlib>

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
			"rol    $0x11,%0\n"
	: "=g" (ret)
	: "0" (addr));
	return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%gs:0x18,%0\n"
			"rol    $0x9,%0\n"
	: "=g" (ret)
	: "0" (addr));
	return ret;
}

#endif


using std::endl;

#define READY 1
#define RUNIN 2
#define SLEEP 3
#define BLOCK 4
#define STACK_SIZE 4096


class uthread{
public:

	/*
     * null constructor
     */
	uthread(): _tid(-1), _stat(-1){}

	
	/*
     * constructor
     */
	uthread(int tid):	_tid(tid), _stat(READY),
						_q_count(0), _s_count(0){}

	char _tid;
	char _stat;
	int _q_count;
	int _s_count;
	char _sp[STACK_SIZE];
	sigjmp_buf _env;

};
