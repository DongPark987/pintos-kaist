#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
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

/* Get Thread List entry */
#define thread_entry(t) list_entry(t, struct thread, elem)
#define donate_entry(t) list_entry(t, struct thread, d_elem)

/* Fixed-Point Real Arithmetic */
#define F 16384                // 17.14 fixed-point
#define int_to_fix(x) (x * F)  // convert to fixed-point
#define fix_to_int(x) (x >= 0) ? ((x + F / 2) / F) : ((x - F / 2) / F)
#define fix_mul_fix(x, y) (((int64_t)x) * y / F)  // to get (x*y) * f
#define fix_div_fix(x, y) (((int64_t)x) * F / y)  // to get (x/y) * f
#define fix_add_int(x, n) (x + n * F)             // x + n
#define fix_sub_int(x, n) (x - n * F)             // x - n

/* Max file descriptor */
#define MAX_FD 128
#define MIN_FD 2
#define STDIN_FILENO 0
#define STDOUT_FILENO 1

/* Terminated children list. */
struct thread_child {
  tid_t tid;
  uint64_t rtn;
  struct list_elem elem;
};

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
 *      1 kB +---------------------------------+
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
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */

  int priority;            /* 나의 priority */
  uint8_t donate_list[64]; /* 기증받은 priority */
  struct thread *holder;   /* 내가 donate했던 애 */

  int nice;       /* for mlfqs */
  int recent_cpu; /* for mlfqs */

  struct list_elem elem;   /* List element. */
  struct list_elem a_elem; /* All thread list */

  uint64_t wake_tick; /* Wake Tick */

  // #ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint64_t *pml4;    /* Page map level 4 */
  uint8_t fd_cnt;    /* Used file descriptor */
  struct file **fdt; /* File descriptor table */

  struct thread *parent;
  struct list exit_children;
  //   struct list wait_children;
  struct semaphore fork_sema; /* Used for process fork */
  struct semaphore wait_sema; /* Used for process fork */
// #endif
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

/* for thread sleep */
void thread_sleep(uint64_t ticks);
void thread_wake(uint64_t ticks);

/* priority */
int thread_get_priority(void);
void thread_set_priority(int);
void thread_reorder(struct thread *);
int get_priority(struct thread *);
void thread_set_recent_cpu(void);

/* mlfq */
int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);
void thread_set_load_avg(void);
int get_recent_cpu(struct thread *);
void set_recent_cpu(struct thread *);
void thread_incr_recent_cpu(void);

int calc_priority(struct thread *);

void thread_reset_priority(void);

/* list cmp funtcions */
bool less_tick(struct list_elem *, struct list_elem *, void *);
bool high_prio(struct list_elem *, struct list_elem *, void *);
bool high_sema(struct list_elem *, struct list_elem *, void *);
bool less_recent(struct list_elem *, struct list_elem *, void *);

void do_iret(struct intr_frame *tf);

/* iterate */
void iterate_recent_cpu(struct list_elem *, void *);

#endif /* threads/thread.h */
