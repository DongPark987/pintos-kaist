#include "threads/thread.h"

#include <debug.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
#define MAX_LIST 64
static struct list ready_list[MAX_LIST];
static struct list all_thread;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests. */
static struct list destruction_req;

/* Thread sleep requests. */
static struct list sleep_list;

/* Statistics. */
static long long idle_ticks;   /* g: num of timer ticks spent idle. */
static long long kernel_ticks; /* g: num of timer ticks in kernel threads. */
static long long user_ticks;   /* g: num of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4          /* m: num of timer ticks to give each thread. */
static unsigned thread_ticks; /* g: num of timer ticks since last yield. */

/* Multilevel feedback queue. */
bool thread_mlfqs;
int ready_threads;
int load_avg;
static struct lock create_lock;

static void kernel_thread(thread_func *, void *aux);
static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

void thread_init(void) {
  ASSERT(intr_get_level() == INTR_OFF);

  /* Reload the temporal gdt for the kernel
   * This gdt does not include the user context.
   * The kernel will rebuild the gdt with user context, in gdt_init (). */
  struct desc_ptr gdt_ds = {
      .size = sizeof(gdt) - 1, /* */
      .address = (uint64_t)gdt /* */
  };
  lgdt(&gdt_ds);

  /* Init the global thread context */
  lock_init(&tid_lock);
  list_init(&sleep_list);
  list_init(&all_thread);
  list_init(&destruction_req);

  for (int i = 0; i < MAX_LIST; i++) {
    list_init(&ready_list[i]);
  }

  /* Init values for mlfqs. */
  load_avg = 0;
  ready_threads = 1;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread();
  init_thread(initial_thread, "main", PRI_DEFAULT);

  if (!list_find(&all_thread, &initial_thread->a_elem)) {
    list_push_back(&all_thread, &initial_thread->a_elem);
  }

  list_init(&initial_thread->children);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid();
}

void thread_start(void) {
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init(&idle_started, 0);
  thread_create("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down(&idle_started);
}

void thread_tick(void) {
  struct thread *t = thread_current();

  /* Update statistics. */
  if (t == idle_thread) {
    idle_ticks++;
  }
#ifdef USERPROG
  else if (t->pml4 != NULL)
    user_ticks++;
#endif
  else {
    kernel_ticks++;
  }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE) {
    intr_yield_on_return();
  }
}

/* Prints thread statistics. */
void thread_print_stats(void) {
  printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
         idle_ticks, kernel_ticks, user_ticks);
}

/* Create a new thread. */
tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
  struct thread_child *child;
  struct thread *t;
  tid_t tid;

  ASSERT(function != NULL);

  /* Allocate thread. */
  t = palloc_get_page(PAL_ZERO);
  if (t == NULL) return TID_ERROR;

  /* Initialize thread. */
  init_thread(t, name, priority);
  tid = t->tid = allocate_tid();

  /* Call the kernel_thread if it scheduled. */
  t->tf.rip = (uintptr_t)kernel_thread;
  t->tf.R.rdi = (uint64_t)function;
  t->tf.R.rsi = (uint64_t)aux;
  t->tf.ds = SEL_KDSEG;
  t->tf.es = SEL_KDSEG;
  t->tf.ss = SEL_KDSEG;
  t->tf.cs = SEL_KCSEG;
  t->tf.eflags = FLAG_IF;

  /* Add to all_thread list for mlfqs. */
  if (!list_find(&all_thread, &t->a_elem)) {
    list_push_back(&all_thread, &t->a_elem);
  }

  /* Add to parent's child list. */
  child = calloc(1, sizeof(struct thread_child));
  if (child == NULL) {
    palloc_free_page(t);
    return TID_ERROR;
  }

  child->addr = t;
  child->tid = t->tid;
  child->status = CHILD_BASE;
  list_push_back(&thread_current()->children, &child->elem);

  /* Add to run queue. */
  thread_unblock(t);

  /* Yield CPU if higher priority. */
  if (t->priority > thread_current()->priority) {
    thread_yield();
  }

  return tid;
}

/* Sleep current thread until WAKE_TICK. */
void thread_sleep(uint64_t wake_tick) {
  struct thread *t;
  enum intr_level old_level;

  ASSERT(intr_get_level() == INTR_ON);
  ASSERT(!intr_context());

  old_level = intr_disable();
  t = thread_current();

  /* Add to sleep queue. */
  if (t != idle_thread) {
    t->wake_tick = wake_tick;
    list_insert_ordered(&sleep_list, &t->elem, less_tick, NULL);
  }

  thread_block();
  intr_set_level(old_level);
}

/* For every timer interrupt, check sleep queue and wake. */
void thread_wake(uint64_t ticks) {
  ASSERT(intr_get_level() == INTR_OFF);
  ASSERT(intr_context());

  enum intr_level old_level;
  old_level = intr_disable();

  struct thread *curr;
  while (!list_empty(&sleep_list)) {
    curr = thread_entry(list_front(&sleep_list));

    if (curr->wake_tick <= ticks) {
      list_pop_front(&sleep_list);
      thread_unblock(curr);
    } else {
      break;
    }
  }
  intr_set_level(old_level);
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block(void) {
  ASSERT(!intr_context());
  ASSERT(intr_get_level() == INTR_OFF);

  if (thread_current() != idle_thread) {
    ready_threads--;
  }

  thread_current()->status = THREAD_BLOCKED;
  schedule();
}

void thread_unblock(struct thread *t) {
  enum intr_level old_level;

  ASSERT(is_thread(t));
  ASSERT(t->status == THREAD_BLOCKED);

  old_level = intr_disable();
  list_insert_ordered(&ready_list[get_priority(t)], &t->elem, less_recent,
                      NULL);

  t->status = THREAD_READY;

  ready_threads++;
  intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *thread_name(void) { return thread_current()->name; }

struct thread *thread_current(void) {
  struct thread *t = running_thread();

  ASSERT(is_thread(t));
  ASSERT(t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void) { return thread_current()->tid; }

/* Terminate thread and add to destruction queue. */
void thread_exit(void) {
  ASSERT(!intr_context());
  /* Just set our status to dying and schedule another process.
    We will be destroyed during the call to schedule_tail(). */
  intr_disable();

#ifdef USERPROG
  process_exit();
#endif

  ready_threads--;
  list_remove(list_find(&all_thread, &thread_current()->a_elem));

  do_schedule(THREAD_DYING);
  NOT_REACHED();
}

/* Yield CPU. */
void thread_yield(void) {
  struct thread *curr = thread_current();
  enum intr_level old_level;

  ASSERT(!intr_context());
  old_level = intr_disable();

  if (curr != idle_thread) {
    list_insert_ordered(&ready_list[get_priority(curr)], &curr->elem,
                        less_recent, NULL);
  }

  do_schedule(THREAD_READY);
  intr_set_level(old_level);
}

/* Increase current thread's recent cpu by 1. */
void thread_incr_recent_cpu(void) {
  /* Update recent_cpu of curr thread */
  if (thread_current() != idle_thread) {
    thread_current()->recent_cpu = fix_add_int(thread_current()->recent_cpu, 1);
  }
}

// * Sets all threads' recent cpu
void thread_set_recent_cpu(void) {
  list_iterate(&all_thread, iterate_recent_cpu, NULL);
  return;
}

// * Set a single thread's recent cpu
void set_recent_cpu(struct thread *t) {
  int coeff = fix_div_fix((2 * load_avg), fix_add_int((2 * load_avg), 1));
  t->recent_cpu = fix_add_int(fix_mul_fix(coeff, t->recent_cpu), t->nice);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
  thread_current()->priority = new_priority;

  /* 우선순위가 높은 스레드가 들어오면 양도하기 위해. 자기자신이 가장
   * 우선순위가 높으면 context switching (thread_launch)이 발생하지 않음 */
  thread_yield();
}

/* Returns the current thread's priority. */
int thread_get_priority(void) {
  // wrapped
  return get_priority(thread_current());
}

/* Returns the specific thread's max priority */
int get_priority(struct thread *t) {
  for (int i = PRI_MAX; i >= PRI_MIN && i > t->priority && !thread_mlfqs; i--) {
    if (t->donate_list[i] > 0) {
      return i;
    }
  }
  return t->priority;
}

// * 우선순위를 계산합니다
int calc_priority(struct thread *t) {
  int new_priority = PRI_MAX - ((t->recent_cpu / 4) / F) - (t->nice * 2);

  /* adjust priority to lie in valide range */
  if (new_priority < PRI_MIN) {
    new_priority = PRI_MIN;
  } else if (new_priority > PRI_MAX) {
    new_priority = PRI_MAX;
  }

  return new_priority;
}

static void iterate_reset_priority(struct list_elem *curr, void *aux UNUSED) {
  struct thread *t = list_entry(curr, struct thread, a_elem);
  t->priority = calc_priority(t);

  if (t->status == THREAD_READY) {
    list_remove(&t->elem);
    list_insert_ordered(&ready_list[t->priority], &t->elem, less_recent, NULL);
  }
}

// * 모든 스레드의 우선순위를 다시 계산하여 queue에 재배치합니다
void thread_reset_priority(void) {
  // reset_priority: priority를 다시 계산하여 ready_list에 재배치
  list_iterate(&all_thread, iterate_reset_priority, NULL);
}

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice) {
  struct thread *curr = thread_current();
  int new_priority;

  curr->nice = nice;
  set_recent_cpu(curr);  // TODO: 필요할까?

  new_priority = calc_priority(curr);
  thread_set_priority(new_priority);
}

/* Returns the current thread's nice value. */
int thread_get_nice(void) { return thread_current()->nice; }

void thread_set_load_avg(void) {
  // load_avg는 고정 소수점 형식
  load_avg =
      fix_mul_fix(fix_div_fix(int_to_fix(59), int_to_fix(60)), load_avg) +
      (fix_div_fix(int_to_fix(1), int_to_fix(60)) * ready_threads);

  ASSERT(load_avg >= 0);
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void) {
  // see thread.h
  return fix_to_int(load_avg * 100);
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
  // multiply 100, round to closest int
  return get_recent_cpu(thread_current());
}

int get_recent_cpu(struct thread *t) {
  // multiply 100, round to closest int
  return fix_to_int(t->recent_cpu * 100);
}

// * 유휴 스레드
static void idle(void *idle_started_ UNUSED) {
  struct semaphore *idle_started = idle_started_;

  /* Init idle_thread. */
  idle_thread = thread_current();

  ready_threads--;  // idle_thread는 cnt에서 제외한다
  if (thread_mlfqs) {
    list_remove(&idle_thread->a_elem);
  }

  /* Free thread_child from main's child list. */
  struct thread_child *idle_thread_child =
      thread_get_child(&idle_thread->parent->children, idle_thread->tid);
  list_remove(&idle_thread_child->elem);
  free(idle_thread_child);

  sema_up(idle_started);

  for (;;) {
    /* Let someone else run. */
    intr_disable();
    thread_block();

    /* set interrupt flag, enable interrupt */
    asm volatile("sti; hlt" : : : "memory");
  }
}

/* Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
  ASSERT(function != NULL);

  intr_enable(); /* The scheduler runs with interrupts off. */
  function(aux); /* Execute the thread function. */
  thread_exit(); /* If function() returns, kill the thread. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void init_thread(struct thread *t, const char *name, int priority) {
  ASSERT(t != NULL);
  ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT(name != NULL);

  memset(t, 0, sizeof *t);

  t->status = THREAD_BLOCKED;
  strlcpy(t->name, name, sizeof t->name);
  t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
  t->magic = THREAD_MAGIC;

  /* 기증받은 우선순위 저장하는 배열 */
  for (int i = 0; i < 64; i++) {
    t->donate_list[i] = 0;
  }

  /* 내가 우선순위를 기증한 스레드 */
  t->holder = NULL;

  /* nice 설정 */
  if (t == initial_thread) {
    t->nice = 0;
    t->recent_cpu = 0;
  } else {
    t->nice = thread_current()->nice;
    t->recent_cpu = thread_current()->recent_cpu;
  }

  /* 우선순위 설정 */
  t->priority = thread_mlfqs ? calc_priority(t) : priority;

  /* for user process */
  t->mode = KERN_TASK;
  t->parent = t == initial_thread ? NULL : thread_current();
  list_init(&t->children);
  sema_init(&t->fork_sema, 0);
  sema_init(&t->wait_sema, 0);
}

// * 다음으로 실행될 스레드
static struct thread *next_thread_to_run(void) {
  for (int i = PRI_MAX; i >= PRI_MIN; i--) {
    if (!list_empty(&ready_list[i])) {
      return list_entry(list_pop_front(&ready_list[i]), struct thread, elem);
    }
  }
  return idle_thread;
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf) {
  __asm __volatile(
      /* movq src des
       * rsp기준 +8byte씩 읽어서 레지스터에 저장
       */
      "movq %0, %%rsp\n"  // tf -> rsp
      "movq 0(%%rsp),%%r15\n"
      "movq 8(%%rsp),%%r14\n"
      "movq 16(%%rsp),%%r13\n"
      "movq 24(%%rsp),%%r12\n"
      "movq 32(%%rsp),%%r11\n"
      "movq 40(%%rsp),%%r10\n"
      "movq 48(%%rsp),%%r9\n"
      "movq 56(%%rsp),%%r8\n"
      "movq 64(%%rsp),%%rsi\n"
      "movq 72(%%rsp),%%rdi\n"
      "movq 80(%%rsp),%%rbp\n"
      "movq 88(%%rsp),%%rdx\n"
      "movq 96(%%rsp),%%rcx\n"
      "movq 104(%%rsp),%%rbx\n"
      "movq 112(%%rsp),%%rax\n"

      /* rsp + 120로 초기화 */
      "addq $120,%%rsp\n"

      /* ds, es 뭔가 또 복제 */
      "movw 8(%%rsp),%%ds\n"
      "movw (%%rsp),%%es\n"
      "addq $32, %%rsp\n"

      /* launch thread.
       * interrupt return
       */
      "iretq"
      :
      : "g"((uint64_t)tf)
      : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void thread_launch(struct thread *th) {
  uint64_t tf_cur = (uint64_t)&running_thread()->tf;  // running_thread
  uint64_t tf = (uint64_t)&th->tf;                    // next thread
  ASSERT(intr_get_level() == INTR_OFF);

  /* The main switching logic.
   * We first restore the whole execution context into the intr_frame
   * and then switching to the next thread by calling do_iret.
   * Note that, we SHOULD NOT use any stack from here
   * until switching is done. */

  __asm __volatile(
      /* Store registers that will be used. */
      "push %%rax\n"
      "push %%rbx\n"
      "push %%rcx\n"

      /* Fetch input once */
      "movq %0, %%rax\n" /* %0은 tf_cur */
      "movq %1, %%rcx\n" /* %1은 tf */

      /* running thread에 next thread frame을 옮김 */
      "movq %%r15, 0(%%rax)\n"
      "movq %%r14, 8(%%rax)\n"
      "movq %%r13, 16(%%rax)\n"
      "movq %%r12, 24(%%rax)\n"
      "movq %%r11, 32(%%rax)\n"
      "movq %%r10, 40(%%rax)\n"
      "movq %%r9, 48(%%rax)\n"
      "movq %%r8, 56(%%rax)\n"
      "movq %%rsi, 64(%%rax)\n"
      "movq %%rdi, 72(%%rax)\n"
      "movq %%rbp, 80(%%rax)\n"
      "movq %%rdx, 88(%%rax)\n"

      "pop %%rbx\n"  // Saved rcx
      "movq %%rbx, 96(%%rax)\n"
      "pop %%rbx\n"  // Saved rbx
      "movq %%rbx, 104(%%rax)\n"
      "pop %%rbx\n"  // Saved rax
      "movq %%rbx, 112(%%rax)\n"
      "addq $120, %%rax\n"

      "movw %%es, (%%rax)\n"
      "movw %%ds, 8(%%rax)\n"
      "addq $32, %%rax\n"
      "call __next\n"  // read the current rip.

      /* next */
      "__next:\n"
      "pop %%rbx\n"
      "addq $(out_iret -  __next), %%rbx\n"
      "movq %%rbx, 0(%%rax)\n"  // rip
      "movw %%cs, 8(%%rax)\n"   // cs
      "pushfq\n"
      "popq %%rbx\n"
      "mov %%rbx, 16(%%rax)\n"  // eflags
      "mov %%rsp, 24(%%rax)\n"  // rsp
      "movw %%ss, 32(%%rax)\n"
      "mov %%rcx, %%rdi\n"
      "call do_iret\n"  // do iret을 호출

      /* out_iret */
      "out_iret:\n"
      :
      : "g"(tf_cur), "g"(tf)
      : "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void do_schedule(int status) {
  ASSERT(intr_get_level() == INTR_OFF);
  ASSERT(thread_current()->status == THREAD_RUNNING);

  while (!list_empty(&destruction_req)) {
    struct thread *victim = thread_entry(list_pop_front(&destruction_req));
    palloc_free_page(victim);
  }
  thread_current()->status = status;
  schedule();
}

/* schedule
  다음 스레드를 ready_list에서 pop해서 실행한다 */
static void schedule(void) {
  struct thread *curr = running_thread();
  struct thread *next = next_thread_to_run();

  ASSERT(intr_get_level() == INTR_OFF);
  ASSERT(curr->status != THREAD_RUNNING);
  ASSERT(is_thread(next));
  /* Mark us as running. */
  next->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;  // 초기화

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate(next);
#endif

  if (curr != next) {
    if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
      ASSERT(curr != next);
      list_push_back(&destruction_req, &curr->elem);
    }
    thread_launch(next);
  }
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire(&tid_lock);
  tid = next_tid++;
  lock_release(&tid_lock);

  return tid;
}

/* re-order ready list */
void thread_reorder(struct thread *t) {
  ASSERT(t->status == THREAD_READY);
  list_remove(&t->elem);
  list_insert_ordered(&ready_list[get_priority(t)], &t->elem, less_recent,
                      NULL);
}

/* Search for child by tid in parent's chlid list.
 * Used for thread fork & wait.
 */
struct thread_child *thread_get_child(struct list *list, tid_t tid) {
  struct list_elem *curr_elem;
  struct thread_child *curr;
  for (curr_elem = list_begin(list); curr_elem != list_end(list);
       curr_elem = list_next(curr_elem)) {
    curr = list_entry(curr_elem, struct thread_child, elem);
    if (curr->tid == tid) {
      return curr;
    }
  }
  return NULL;
}

/* Iterate list and recalculate recent cpu. */
void iterate_recent_cpu(struct list_elem *elem, void *aux UNUSED) {
  struct thread *t = list_entry(elem, struct thread, a_elem);
  set_recent_cpu(t);
}

/* list comparison functions */
bool high_prio(struct list_elem *_a, struct list_elem *_b, void *) {
  struct thread *a = thread_entry(_a);
  struct thread *b = thread_entry(_b);
  return (get_priority(a) > get_priority(b));
}

bool less_tick(struct list_elem *_a, struct list_elem *_b, void *aux) {
  struct thread *a = thread_entry(_a);
  struct thread *b = thread_entry(_b);
  return (a->wake_tick < b->wake_tick);
}

bool less_recent(struct list_elem *_a, struct list_elem *_b, void *aux) {
  struct thread *a = thread_entry(_a);
  struct thread *b = thread_entry(_b);
  return (a->recent_cpu < b->recent_cpu);
}