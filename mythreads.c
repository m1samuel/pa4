/*	User-level thread system
 *      
 */

#include <setjmp.h>

#include "aux.h"
#include "umix.h"
#include "mythreads.h"
#include "string.h"

static int MyInitThreadsCalled = 0;	/* 1 if MyInitThreads called, else 0 */
static int CurrentThread = 0;
static int curr = 0;
static struct thread {			/* thread table */
  int valid;			/* 1 if entry is valid, else 0 */
  jmp_buf env;			/* current context */
  jmp_buf clean_env;
  void (*func)();
  int param;
} thread[MAXTHREADS];

static struct {
  int prev;
  int next;
} q[MAXTHREADS];

static int front = -1;
static int back = -1;
static int size = 0;

void insertFront(int n){
  q[n].prev = -1;
  q[n].next = front;
  if (size != 0) q[front].prev = n;
  else back = n;
  front = n;
  size++;
}

void insertRear(int n){
  q[n].prev = back;
  q[n].next = -1;
  if (size != 0) q[back].next = n;
  else front = n;
  back = n;
  size++;
}

void remove(int n) {
  if (size==1){
    front=-1;
    back=-1;
  } else if (front==n){
    front = q[n].next;
    q[front].prev=-1;
  } else if (back==n){
    back = q[n].prev;
    q[back].next = -1;
  } else {
    q[q[n].prev].next = q[n].next;
    q[q[n].next].prev = q[n].prev;
  }
  size--;
}
#define STACKSIZE	65536		/* maximum size of thread stack */

/*	MyInitThreads () initializes the thread package. Must be the first
 *	function called by any user program that uses the thread package.  
 */
void stackPartition(int n){
  if (n<=1) return;
  else {
    char s[STACKSIZE];
    if (((int) &s[STACKSIZE-1]) - ((int) &s[0]) + 1 != STACKSIZE) Exit();
    if (setjmp(thread[MAXTHREADS - n + 1].env) == 0) {
      memcpy(thread[MAXTHREADS - n + 1].clean_env, thread[MAXTHREADS - n + 1].env, sizeof(jmp_buf));
      stackPartition(n-1);
    } else {
      (*thread[CurrentThread].func) (thread[CurrentThread].param);
      MyExitThread();
    }
  }
}
void MyInitThreads ()
{
  int i,j,k;

	if (MyInitThreadsCalled) {                /* run only once */
		Printf ("InitThreads: should be called only once\n");
		Exit ();
	}

	for (i = 0; i < MAXTHREADS; i++) {	/* initialize thread table */
		thread[i].valid = 0;
		q[i].prev = -1;
		q[i].next = -1;
	}

	if (setjmp(thread[0].env) == 0) {
	  memcpy(thread[0].clean_env, thread[0].env, sizeof(jmp_buf));
	  stackPartition(MAXTHREADS);
	  thread[0].valid = 1;
	  insertFront(0);
	} else {
	  (*thread[0].func) (thread[0].param);
	  MyExitThread();
	}
	MyInitThreadsCalled = 1;
}

/*	MySpawnThread (func, param) spawns a new thread to execute
 *	func (param), where func is a function with no return value and
 *	param is an integer parameter.  The new thread does not begin
 *	executing until another thread yields to it.  
 */

int MySpawnThread (func, param)
	void (*func)();		/* function to be executed */
	int param;		/* integer parameter */
{
	if (!MyInitThreadsCalled) {
		Printf ("SpawnThread: Must call InitThreads first\n");
		Exit ();
	}
	int index = (curr+1) % MAXTHREADS;
	int i = 0;
	while (i < MAXTHREADS){ 
	  if (!thread[index].valid) break;
	  index = (index+1) % MAXTHREADS;
	  i++;
	}
	/*i = front;
	while (i != back){
	  Printf("i: %d", i);
	  i = q[i].next;
	}
	Printf("\n");
	*/
	//Printf("i: %d\n", i);   
	if (thread[index].valid && i==MAXTHREADS) return -1;
	curr = index;
	//Printf("curr: %d\n", curr);
	//Printf("thread[0].valid: %d\n", thread[0].valid);
	memcpy(thread[curr].env, thread[curr].clean_env, sizeof(jmp_buf));
	thread[curr].valid = 1;
	thread[curr].func = func;
	thread[curr].param = param;
	insertRear(curr);
	//Printf("MyspawnThread\n");
	return (curr);		/* done spawning, return new thread ID */
}

/*	MyYieldThread (t) causes the running thread, call it T, to yield to
 *	thread t.  Returns the ID of the thread that yielded to the calling
 *	thread T, or -1 if t is an invalid ID.  Example: given two threads
 *	with IDs 1 and 2, if thread 1 calls MyYieldThread (2), then thread 2
 *	will resume, and if thread 2 then calls MyYieldThread (1), thread 1
 *	will resume by returning from its call to MyYieldThread (2), which
 *	will return the value 2.
 */

int MyYieldThread (t)
     int t;			/* thread being yielded to */
{
  if (! MyInitThreadsCalled) {
    Printf ("YieldThread: Must call InitThreads first\n");
    Exit ();
  }
  
  if (t < 0 || t >= MAXTHREADS) {
    Printf ("YieldThread: %d is not a valid thread ID\n", t);
    return (-1);
  }
  if (! thread[t].valid) {
    Printf ("YieldThread: Thread %d does not exist\n", t);
    return (-1);
  }
  //Printf("\nq[3].next: %d\n", q[3].next);
  //Printf("q[4].prev: %d\n", q[4].prev);
  //int i = front;
  //Printf("\nMyYield before\n");
  //while (i != back){
  //  Printf("i: %d", i);
  //  i = q[i].next;
  //}
  //Printf("i: %d", i);
  if (t==CurrentThread) return t;
  remove(t);
  insertFront(t);
  if (thread[CurrentThread].valid){
    remove(CurrentThread);
    insertRear(CurrentThread);
  }
  /*Printf("\nMyYield after\n");
  i = front;
  while (i != back){
    Printf("i: %d", i);
    i = q[i].next;
  }
  Printf("i: %d", i);
  Printf("\n");*/
  int r;
  static int c; 
  c = CurrentThread;
  if ((r = setjmp(thread[CurrentThread].env)) == 0) {
    CurrentThread = t;
    //Printf("########## temp value is %d ######## \n", temp);
    longjmp (thread[t].env, t+1);
  }
  return c;
}

/*	MyGetThread () returns ID of currently running thread. 
 */

int MyGetThread ()
{
	if (! MyInitThreadsCalled) {
		Printf ("GetThread: Must call InitThreads first\n");
		Exit ();
	}
	return CurrentThread;
}

/*	MySchedThread () causes the running thread to simply give up the
 *	CPU and allow another thread to be scheduled. Selecting which
 *	thread to run is determined here. Note that the same thread may
 * 	be chosen (as will be the case if there are no other threads).  
 */

void MySchedThread ()
{
  if (! MyInitThreadsCalled) {
    Printf ("SchedThread: Must call InitThreads first\n");
    Exit ();
  }
  if (size==1) MyYieldThread(CurrentThread);
  else MyYieldThread(q[front].next);
}

/*	MyExitThread () causes the currently running thread to exit.  
 */

void MyExitThread ()
{
	if (! MyInitThreadsCalled) {
		Printf ("ExitThread: Must call InitThreads first\n");
		Exit ();
	}
	thread[CurrentThread].valid=0;
	remove(CurrentThread);
	if (size==0) Exit();
	MyYieldThread(front);
}
