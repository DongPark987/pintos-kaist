#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

/* Fixed-Point Real Arithmetic */

// Convert n to fixed point: n * f
#define INT_TO_FIXED(n) ((n) * F)

// Convert x to integer (rounding toward zero): x / f
#define FIXED_TO_INT_ZERO(x) ((x) / F)

// Convert x to integer (rounding to nearest):
#define FIXED_TO_INT_NEAREST(x) (((x) >= 0) ? (((x) + F / 2) / F) : (((x) - F / 2) / F))

// Add x and y: x + y
#define FIXED_ADD(x, y) ((x) + (y))

// Subtract y from x: x - y
#define FIXED_SUBTRACT(x, y) ((x) - (y))

// Add x and n: x + n * f
#define FIXED_ADD_INT(x, n) ((x) + (n) * F)

// Subtract n from x: x - n * f
#define FIXED_SUBTRACT_INT(x, n) ((x) - (n) * F)

// Multiply x by y: ((int64_t) x) * y / f
#define FIXED_MULTIPLY(x, y) (((int64_t) (x)) * (y) / F)

// Multiply x by n: x * n
#define FIXED_MULTIPLY_INT(x, n) ((x) * (n))

// Divide x by y: ((int64_t) x) * f / y
#define FIXED_DIVIDE(x, y) (((int64_t) (x)) * F / (y))

// Divide x by n: x / n
#define FIXED_DIVIDE_INT(x, n) ((x) / (n))

#define F (1 << 14) // 2^14, 고정 소수점 표현에서 사용하는 값

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
  int origin_priority;
  struct lock *wait_on_lock;
  struct list donators;
  
  int64_t wake_tick;
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
  struct list_elem d_elem;            /* donate list element */
  struct list_elem a_elem;            /* All list element */

  /* mlfqs */
  int nice;
  int recent_cpu; // the amount of CPU time a thread has received recently

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
  /* Table for whole virtual memory owned by thread. */
  struct supplemental_page_table spt;
#endif

  /* Owned by thread.c. */
  struct intr_frame tf; /* Information for switching */
  unsigned magic;       /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
void thread_set_recent_cpu(void);
int thread_get_load_avg(void);
void thread_set_load_avg(void);

void do_iret (struct intr_frame *tf);

/*
	timer_sleep() 구현
*/
bool cmp_wake_tick(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool cmp_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
bool cmp_donate_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
bool cmp_less_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
bool cmp_less_donate_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
bool cmp_recent_cpu(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

void thread_sleep(int64_t ticks);
void thread_wake(int64_t ticks);

/*
	priority
*/
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void calculate_priority(struct thread *t);
int find_highest_priority(void);

#endif /* threads/thread.h */
