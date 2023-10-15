#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
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
   that are ready to run but not actually running.

   for mlfqs */
static struct list ready_list[64];

/* All threads */
static struct list all_list;

/*sleep List*/
static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4		  /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/* mlfq용 변수 선언 */
static int ready_threads;
static int load_avg;

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

/* 고정 소수점 계산 매크로 f = 2^14 */
#define f 16384

/* 정수를 실수로 */
#define to_fix(n) (n * f)

/* 실수간 나누기 */
#define divide_fix(x, y) (((int64_t)x) * f / y)

/* 실수간 곱셈 */
#define multiply_fix(x, y) (((int64_t)x) * y / f)

/* 실수에서 정수 빼기 */
#define subtract_fix(x, n) (x - n * f)

/* 실수에서 정수 더하기 */
#define add_fix(x, n) (x + n * f)

/* 실수를 정수로 변환(반올림)*/
#define fix_to_int_near(x) x >= 0 ? ((x + f / 2) / f) : ((x - f / 2) / f)

/* 실수를 정수로 변환 (내림) */
#define fix_to_int(x) (x / f)

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes.

   현재 실행 중인 코드를 스레드로 변환하여 스레딩 시스템을 초기화합니다. 이것은 일반적으로 동작하지 않을 수 있으며, 여
   기에서만 가능한 것은 loader.S가 스택의 하단을 페이지 경계에 두기 위해 주의를 기울였기 때문입니다.
   또한 실행 대기열과 tid(스레드 식별자) 잠금을 초기화합니다.

   이 함수를 호출한 후에는 thread_create()를 사용하여 스레드를 생성하기 전에 페이지 할당자를 초기화해야 합니다.

   이 함수가 완료될 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. */

void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the globla thread context */
	lock_init(&tid_lock);
	for (int i = 0; i < 64; i++)
		list_init(&ready_list[i]);
	list_init(&sleep_list);
	list_init(&destruction_req);

	/* mlfq용 모든 쓰레드 연결 리스트 초기화 */
	list_init(&all_list);

	/* 1분간 Running, Ready 상태였던 쓰레드의 추정치 */
	load_avg = 0;
	/* 현재 Running, Ready 상태인 쓰레드의 수 (idle 제외) */
	ready_threads = 1;

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread.

   인터럽트를 활성화함으로써 선점형 스레드 스케줄링을 시작합니다. 또한 아이들 스레드를 생성합니다. */
void thread_start(void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context.
   타이머 인터럽트 핸들러에 의해 각 타이머 틱마다 호출됩니다. 따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
void thread_tick(void)
{
	struct thread *t = thread_current();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}

/* current_thread의 recent_cpu 1 증가 (1틱마다) */
void thread_recent_cpu_incr(void)
{
	struct thread *t = thread_current();
	if (t != idle_thread)
		t->recent_cpu = add_fix(t->recent_cpu, 1);
}

/* load_avg 갱신 (100틱마다) */
void thread_calc_load(void)
{
	load_avg = multiply_fix(divide_fix(to_fix(59), to_fix(60)), load_avg) + divide_fix(to_fix(1), to_fix(60)) * ready_threads;
}

/* 모든 쓰레드의 recent_cpu 갱신 (100틱마다) */
void thread_calc_recent_cpu(void)
{
	for (struct list_elem *cur = list_begin(&all_list); cur != list_end(&all_list); cur = list_next(cur))
	{
		struct thread *t = list_entry(cur, struct thread, all_elem);
		t->recent_cpu = add_fix((multiply_fix(divide_fix((2 * load_avg), (add_fix((2 * load_avg), 1))), t->recent_cpu)), t->nice);
	}
}

/* Prints thread statistics. 스레드 통계를 출력합니다. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3.

   주어진 초기 우선순위(PRIORITY)로 이름이 NAME인 새로운 커널 스레드를 생성하고,
   FUNCTION을 인자로 AUX를 전달하여 실행하며, 이를 준비 큐(ready queue)에 추가합니다.
   새로운 스레드의 식별자(thread identifier)를 반환하며, 생성에 실패한 경우 TID_ERROR를 반환합니다.

   만약 thread_start()가 호출된 경우, 새로운 스레드는 thread_create()가 반환하기 전에 예약될 수 있습니다.
   또한 thread_create()가 반환되기 전에 새로운 스레드가 종료될 수도 있습니다.
   반대로, 원래의 스레드는 새로운 스레드가 예약되기 전까지 어떤 시간 동안이라도 실행될 수 있습니다.
   순서를 보장해야 하는 경우 세마포어 또는 기타 형태의 동기화를 사용하십시오.

   제공된 코드는 새로운 스레드의 `priority` 멤버를 PRIORITY로 설정하지만, 실제 우선순위 스케줄링은 구현되어 있지 않습니다.
   우선순위 스케줄링은 Problem 1-3의 목표입니다. */

tid_t thread_create(const char *name, int priority,
					thread_func *function, void *aux)
{
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL){
		return TID_ERROR;
	}

	/* Initialize thread. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	/* 커널 스레드를 호출합니다. 스케줄링되어야 합니다.
	 * 참고) rdi는 첫 번째 인수이고, rsi는 두 번째 인수입니다. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	t->parent = thread_current();
	struct child_info *child_info = malloc(sizeof(struct child_info));
	if(child_info == NULL){
		palloc_free_page(t);
		return TID_ERROR;
	}
	child_info->pid = t->tid;
	child_info->status = CHILD_RUNNING;
	child_info->child_thread = t;

	/* 부모에게 알리는 자신의 상태 정보 연결 */
	t->child_info = child_info;
	list_push_back(&t->parent->child_list, &child_info->child_elem);
	thread_unblock(t);
	thread_yield();

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h.
   현재 스레드를 슬립 모드로 전환합니다. 스레드는 thread_unblock()에 의해 깨어날 때까지 다시 스케줄되지 않습니다.

   이 함수는 인터럽트가 비활성화된 상태에서 호출되어야 합니다.
   일반적으로 synch.h에서 제공하는 동기화 원시(primitives) 중 하나를 사용하는 것이 더 나은 아이디어입니다. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);

	/* mlfq */
	if (thread_current() != idle_thread && thread_mlfqs)
	{
		ready_threads--;
		ASSERT(ready_threads >= 0);
	}
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data.

   차단된 스레드 T를 실행 가능한 상태로 전환합니다.
   T가 차단되지 않았다면 오류가 발생합니다.
   (실행 중인 스레드를 실행 가능하게 하려면 thread_yield()를 사용하세요.)

   이 함수는 실행 중인 스레드를 선점하지 않습니다.
   이것은 중요할 수 있습니다: 호출자가 직접 인터럽트를 비활성화한 경우 스레드를 원자적으로
   차단 해제하고 다른 데이터를 업데이트할 수 있다고 기대할 수 있기 때문입니다. */

void thread_unblock(struct thread *t)
{
	enum intr_level old_level;

	ASSERT(is_thread(t));

	old_level = intr_disable();
	ASSERT(t->status == THREAD_BLOCKED);
	if (thread_mlfqs)
		list_insert_ordered(&ready_list[t->priority], &t->elem, cmp_recent, NULL);
	else
		list_insert_ordered(&ready_list, &t->elem, cmp_priority, NULL);

	t->status = THREAD_READY;
	/* mlfq */
	if (thread_mlfqs)
		ready_threads++;
	intr_set_level(old_level);
}

/* Returns the name of the running thread.
   쓰레드 이름 반환*/
const char *
thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details.

   실행 중인 스레드를 반환합니다.
   이것은 running_thread()와 몇 가지 안전성 확인을 추가한 것입니다.
   자세한 내용은 thread.h 맨 위의 큰 주석을 참조하세요.*/
struct thread *
thread_current(void)
{
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow.

	   T가 실제로 스레드인지 확인하세요.
	   두 개의 어서션 중 하나라도 발생하면 스레드가 스택 오버플로우에 빠졌을 수 있습니다.
	   각 스레드는 4KB 미만의 스택을 갖고 있으므로 큰 자동 배열 몇 개나 중간 정도의
	   재귀 호출만으로도 스택 오버플로우가 발생할 수 있습니다. */

	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);
	return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable();
	/* mlfqs */
	if (thread_mlfqs)
	{
		list_remove(&thread_current()->all_elem);
		ready_threads--;
	}
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim.
   CPU를 양보합니다. 현재 스레드는 잠들지 않으며 스케줄러의 재량에 따라 즉시 다시 예약될 수 있습니다.
   */
void thread_yield(void)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;

	// if (!thread_mlfqs && !list_empty(&ready_list))
	// {
	// 	struct thread *t = list_entry(list_front(&ready_list), struct thread, elem);
	// 	if (thread_current()->priority >= thread_get_priority_manual(t));
	// 		return;
	// }

	ASSERT(!intr_context());

	old_level = intr_disable();
	if (curr != idle_thread)
	{
		/* mlfqs */
		if (thread_mlfqs)
			list_insert_ordered(&ready_list[thread_get_priority()], &curr->elem, cmp_recent, NULL);
		else
			list_insert_ordered(&ready_list, &curr->elem, cmp_priority, NULL);
	}

	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

/*현재 쓰레드가 THREAD_READY 되어야 할 wakeTick을 설정하고
block상태로 전환 밑 sleep_list에 추가 */
void thread_sleep(int64_t ticks)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;
	ASSERT(!intr_context());

	old_level = intr_disable();

	if (curr != idle_thread)
	{
		curr->wake_tick = ticks;
		list_insert_ordered(&sleep_list, &curr->elem, cmp_wake_tick, NULL);
	}
	thread_block();
	intr_set_level(old_level);
}

/*sleep_list 리스트에서 현재 ticks 보다 작은 wake_tick을 가진 쓰레드 ready 상태로 변경
  ,ready_list로 이동*/
void thread_wake(int64_t ticks)
{
	enum intr_level old_level;
	ASSERT(intr_context());
	old_level = intr_disable();

	struct thread *curr;
	while (!list_empty(&sleep_list))
	{
		curr = list_entry(list_front(&sleep_list), struct thread, elem);
		if (curr->wake_tick <= ticks)
		{
			list_pop_front(&sleep_list);
			thread_unblock(curr);
		}
		else
			break;
	}
	intr_set_level(old_level);
}

void thread_relocate_ready(struct thread *t)
{
	list_remove(&t->elem);
	t->status = THREAD_BLOCKED;
	thread_unblock(t);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority)
{
	ASSERT(!thread_mlfqs);
	thread_current()->priority = new_priority;
	/*더 높은 priority를 가진 thread가 들어오면 자원을 양도하기 위해
	  일단 yield를 수행하고 readylist에서 가장 우선순위가 높은 thread부터
	  실행한다. 자신이 우선순위가 가장 높은 경우 yield가 호출되어도 문맥 교환이
	  일어나지 않는다. */
	thread_yield();
}

/* Returns the current thread's priority. */
int thread_get_priority(void)
{
	return thread_get_priority_manual(thread_current());
}

int thread_get_priority_manual(struct thread *t)
{
	if (thread_mlfqs)
		return t->priority;

	if (t->donation_cnt == 0)
		return t->priority;
	else
	{
		int i = 63;
		for (; i >= 0; i--)
			if (t->donation_list[i] != 0)
				break;
		return i;
	}
}

/* mlfq 에서 쓰레드의 우선 순위 갱신 함수 */
void thread_calc_priority(void)
{
	for (struct list_elem *cur = list_begin(&all_list); cur != list_end(&all_list); cur = list_next(cur))
	{
		struct thread *t = list_entry(cur, struct thread, all_elem);
		t->priority = PRI_MAX - fix_to_int((t->recent_cpu / 4)) - (t->nice * 2);
		if (t->priority < PRI_MIN)
			t->priority = PRI_MIN;
		if (t->priority > PRI_MAX)
			t->priority = PRI_MAX;

		if (t->status != THREAD_READY)
			continue;
		list_remove(&t->elem);
		list_insert_ordered(&ready_list[t->priority], &t->elem, cmp_recent, NULL);
	}
}

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice)
{
	enum intr_level old_level;
	/* priority 갱신중  인터럽트 발생 */
	old_level = intr_disable();
	thread_current()->nice = nice;
	thread_calc_priority();
	intr_set_level(old_level);
	thread_yield();
}

/* Returns the current thread's nice value. */
int thread_get_nice(void)
{
	return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void)
{
	return fix_to_int_near(load_avg * 100);
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void)
{
	return fix_to_int_near(thread_current()->recent_cpu * 100);
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	if (thread_mlfqs)
	{
		ready_threads--;
		idle_thread->priority = 0;
		list_remove(&idle_thread->all_elem);
	}
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		intr_disable();
		thread_block();
		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   "인터럽트를 다시 활성화하고 다음 인터럽트를 기다립니다.

		   'sti' 명령은 다음 명령의 완료까지 인터럽트를 비활성화하므로
		   이 두 개의 명령은 원자적으로 실행됩니다. 이 원자성은 중요합니다.
		   그렇지 않으면 인터럽트가 인터럽트를 다시 활성화하고 다음 인터럽트가
		   발생하기를 기다리는 사이에 처리될 수 있으며, 이로 인해 1클럭 틱만큼의
		   시간이 낭비될 수 있습니다."

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */
	function(aux); /* Execute the thread function. */
	thread_exit(); /* If function() returns, kill the thread. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	if (!thread_mlfqs)
		t->priority = priority;
	else
	{
		if (initial_thread == t)
		{
			t->nice = 0;
			t->recent_cpu = 0;
		}
		else
		{
			t->nice = thread_current()->nice;
			t->recent_cpu = thread_current()->recent_cpu;
		}
		t->priority = PRI_MAX - (fix_to_int((t->recent_cpu / 4))) - (t->nice * 2);
		if (t->priority < PRI_MIN)
			t->priority = PRI_MIN;
		if (t->priority > PRI_MAX)
			t->priority = PRI_MAX;
		list_push_back(&all_list, &t->all_elem);
	}
	t->magic = THREAD_MAGIC;
	t->wait_on_lock = NULL;
	t->donation_cnt = 0;
	for (int i = 0; i < 64; i++)
		t->donation_list[i] = 0;

	/* fork용 변수 초기화 */
	sema_init(&t->fork_sema, 0);
	sema_init(&t->wait_sema, 0);
	list_init(&t->child_list);
	t->exe_file = NULL;
	t->parent = NULL;
	t->child_info = NULL;
	t->fdt = NULL;
	t->is_kernel = 1;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */

static struct thread *next_thread_to_run(void)
{
	if (thread_mlfqs)
	{
		for (int i = PRI_MAX; i >= PRI_MIN; i--)
			if (!list_empty(&ready_list[i]))
				return list_entry(list_pop_front(&ready_list[i]), struct thread, elem);

		return idle_thread;
	}
	else
	{
		if (list_empty(&ready_list))
			return idle_thread;
		else
			return list_entry(list_pop_front(&ready_list), struct thread, elem);
	}
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"
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
		"addq $120,%%rsp\n"
		"movw 8(%%rsp),%%ds\n"
		"movw (%%rsp),%%es\n"
		"addq $32, %%rsp\n"
		"iretq"
		: : "g"((uint64_t)tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   이 함수는 새로운 스레드의 페이지 테이블을 활성화하고,
   이전 스레드가 종료 중인 경우에는 해당 스레드를 파괴하는
   작업을 수행합니다.
   이 함수가 호출될 때, 우리는 이미 이전 스레드 PREV에서 스레드를 전환했으며,
   새로운 스레드가 이미 실행 중이며 인터럽트는 아직 비활성화된 상태입니다.

   이 함수 안에서 printf()를 호출하는 것은 안전하지 않습니다.
   실제로는 스레드 전환이 완료된 이후에 printf()를 추가해야 합니다. */
static void
thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done.
	 * 주요 전환 로직입니다.
	   먼저 실행 컨텍스트 전체를 intr_frame에 복원한 다음 do_iret를 호출하여 다음 스레드로 전환합니다.
	   주의할 점은 여기서부터는 전환이 완료될 때까지 어떠한 스택도 사용해서는 안 된다는 것입니다.*/

	__asm __volatile(
		/* Store registers that will be used. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* Fetch input once */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
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
		"pop %%rbx\n" // Saved rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // Saved rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // Saved rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // read the current rip.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		: : "g"(tf_cur), "g"(tf) : "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);
	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current()->status = status;
	schedule();
}

static void
schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate(next);
#endif

	if (curr != next)
	{
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule().

		   만약 우리가 전환한 스레드가 종료 중이라면, 해당 스레드의 구조체(thread struct)를 파괴합니다.
		   이 작업은 thread_exit() 함수가 자신을 지원하지 않도록하기 위해 늦게 발생해야 합니다.
		   여기서는 페이지가 현재 스택에 의해 사용 중이기 때문에 페이지 해제 요청을 대기열에 추가하기만 합니다.
		   실제 파괴 로직은 schedule() 함수의 시작에서 호출될 것입니다. */

		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch(next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}

/* recent_cpu 비교 */
bool cmp_recent(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
	const struct thread *a = list_entry(a_, struct thread, elem);
	const struct thread *b = list_entry(b_, struct thread, elem);
	return (a->recent_cpu < b->recent_cpu);
}

/*삽입 정렬 시 wake_tick 비교 함수*/
bool cmp_wake_tick(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
	const struct thread *a = list_entry(a_, struct thread, elem);
	const struct thread *b = list_entry(b_, struct thread, elem);
	return (a->wake_tick < b->wake_tick);
}

/* 우선순위 비교 */
bool cmp_priority(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
	const struct thread *a = list_entry(a_, struct thread, elem);
	const struct thread *b = list_entry(b_, struct thread, elem);
	return (thread_get_priority_manual(a) > thread_get_priority_manual(b));
}