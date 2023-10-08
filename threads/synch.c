/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"

#include <stdio.h>
#include <string.h>

#include "threads/interrupt.h"
#include "threads/thread.h"

/* 동기화에서 원자성이란 (atomicity) 일련의 연산들이 하나의 단일 연산처럼
 * 보이도록 하는 특성이다. 완전히 수행하거나, 아예 수행되지 않는 두 가지 상태만
 * 가진다.*/

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any).

   세마포어 SEMA를 VALUE로 초기화합니다. 세마포어는 그것을 조작하기 위한 두 개의
   원자적 연산자와 함께하는 음이 아닌 정수입니다:

   - down 또는 "P": 값이 양수가 될 때까지 기다린 후 감소시킵니다.
   - up 또는 "V": 값을 증가시키며 (있는 경우 대기 중인 스레드 중 하나를
   깨웁니다).
 */
void sema_init(struct semaphore *sema, unsigned value) {
  ASSERT(sema != NULL);

  sema->value = value;
  list_init(&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function.

   세마포어의 down 또는 'P' 연산. `sema`의 값이 양수가 되기를 기다린 후에
   원자적으로 감소시킨다.

   이 함수는 잠들 수 있으므로 인터럽트 핸들러 안에서 호출되면 안된다. 인터럽트가
   비활성화된 상태에서 호출될 수 있지만, 만약 잠든다면 다음으로 스케줄된
   스레드가 인터럽트를 활성화시킬 것이다. */
void sema_down(struct semaphore *sema) {
  enum intr_level old_level;

  ASSERT(sema != NULL);
  ASSERT(!intr_context());

  old_level = intr_disable(); /* 인터럽트 끄기 */
  while (sema->value == 0) { /* 자리가 없으면 wait list에 넣고 블락 */
    list_push_back(&sema->waiters, &thread_current()->elem);
    thread_block();
  }
  sema->value--;             /* 자리가 생기면 들어간다 */
  intr_set_level(old_level); /* 인터럽트 켜기 */
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise. This function may be called from an
   interrupt handler.

   세마포어가 이미 0이 아닐 경우 세마포어에 대한 "Down" 또는
   "P" 연산을 수행합니다. 세마포어가 감소하면 true를 반환하고, 그렇지 않으면
   false를 반환합니다. 이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
bool sema_try_down(struct semaphore *sema) {
  enum intr_level old_level;
  bool success;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (sema->value > 0) {
    sema->value--;
    success = true;
  } else
    success = false;
  intr_set_level(old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.
   This function may be called from an interrupt handler.

   세마포어에 대한 "Up" 또는 "V" 연산을 수행합니다. SEMA의 값을 증가시키고,
   SEMA를 기다리는 스레드 중 하나를 깨웁니다(있는 경우). 이 함수는 인터럽트
   핸들러에서 호출될 수 있습니다. */
void sema_up(struct semaphore *sema) {
  enum intr_level old_level;
  ASSERT(sema != NULL);
  old_level = intr_disable(); /* 인터럽트 끄고 */
  struct list_elem *next_elem = NULL;
  struct thread *next = NULL;

  sema->value++; /* 자리 있음을 알림 */

  if (!list_empty(&sema->waiters)) {
    /* unblock할 애 */
    next_elem = list_min(&sema->waiters, high_prio, NULL);
    next = thread_entry(next_elem);
    list_remove(next_elem);
    thread_unblock(next);

    /* 우선순위 높은 애가 풀려났으면 */
    if (!intr_context() && get_priority(next) > thread_current()->priority) {
      thread_yield();
    }
  }
  intr_set_level(old_level); /* 인터럽트 활성화 */
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void) {
  struct semaphore sema[2];
  int i;

  printf("Testing semaphores...");
  sema_init(&sema[0], 0);
  sema_init(&sema[1], 0);
  thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) {
    sema_up(&sema[0]);
    sema_down(&sema[1]);
  }
  printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void sema_test_helper(void *sema_) {
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) {
    sema_down(&sema[0]);
    sema_up(&sema[1]);
  }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock.

   LOCK을 초기화합니다. 잠금은 주어진 시간에 하나의 스레드에 의해서만 보유될 수
   있습니다. 우리의 잠금은 "재귀적"이 아닙니다, 즉, 현재 잠금을 보유하고 있는
   스레드가 그 잠금을 획득하려고 시도하는 것은 오류입니다.

   잠금은 초기 값이 1인 세마포어의 특수화입니다. 잠금과 이러한 세마포어 사이의
   차이점은 두 가지입니다. 첫째, 세마포어는 1보다 큰 값을 가질 수 있지만, 잠금은
   한 번에 하나의 스레드에 의해서만 소유될 수 있습니다. 둘째, 세마포어는
   소유자가 없습니다, 즉 하나의 스레드가 세마포어를 "down"할 수 있고 그 다음에
   다른 스레드가 "up"할 수 있지만, 잠금을 사용할 때는 동일한 스레드가 획득하고
   해제해야 합니다. 이러한 제한이 부담스러울 때, 잠금 대신 세마포어를 사용해야
   한다는 좋은 징조입니다. */
void lock_init(struct lock *lock) {
  ASSERT(lock != NULL);

  lock->holder = NULL;
  sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep.

   LOCK을 획득합니다. 필요한 경우 사용 가능할 때까지 대기합니다. 이 잠금은 현재
   스레드에 의해 이미 보유되어 있지 않아야 합니다.

   이 함수는 잠들 수 있으므로, 인터럽트 핸들러 내에서 호출되어서는 안됩니다. 이
   함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, 잠들어야 할 경우
   인터럽트는 다시 활성화됩니다. */
void lock_acquire(struct lock *lock) {
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(!lock_held_by_current_thread(lock));

  if (thread_mlfqs) {
    sema_down(&lock->semaphore);     /* waiters or get lock */
    lock->holder = thread_current(); /* 누군가가.. 락 획득 성공 */
    return;
  }

  /* nested priority donation */
  struct thread *holder = lock->holder;
  struct thread *curr = thread_current();

  while (holder) {
    // holder의 original priority보다 크면 donate
    if (curr->priority > holder->priority) {
      holder->donate_list[curr->priority]++;
    }

    // 나의 donate받은 priority가 holder의 original priority보다 크면
    for (int i = PRI_MAX; i >= PRI_MIN; i--) {
      if (curr->donate_list[i] > 0 && i > holder->priority) {
        holder->donate_list[i]++;
      }
    }
    /* lock holder가 ready list에 있으면 */
    if (holder && holder->status == THREAD_READY) {
      thread_reorder(holder);
    }

    holder = holder->holder;
  }

  /* 나의 홀더를 현재 lock->holder로 설정 */
  thread_current()->holder = lock->holder ? lock->holder : NULL;
  sema_down(&lock->semaphore);     /* waiters or get lock */
  lock->holder = thread_current(); /* 누군가가.. 락 획득 성공 */

  /* waiters 중 donation 받아야 될 것 있는지 체크 */
  if (!list_empty(&lock->semaphore.waiters)) {
    curr = list_entry(list_min(&lock->semaphore.waiters, high_prio, NULL),
                      struct thread, elem);
    if (get_priority(curr) > thread_current()->priority) {
      thread_current()->donate_list[get_priority(curr)]++;
    }
  }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler.

   LOCK을 획득하려고 시도하고 성공하면 true를 반환하거나 실패하면 false를
   반환합니다. 이 잠금은 현재 스레드에 의해 이미 보유되어 있지 않아야 합니다.

   이 함수는 잠들지 않으므로, 인터럽트 핸들러 내에서 호출될 수 있습니다. */
bool lock_try_acquire(struct lock *lock) {
  bool success;

  ASSERT(lock != NULL);
  ASSERT(!lock_held_by_current_thread(lock));

  success = sema_try_down(&lock->semaphore);
  if (success) lock->holder = thread_current();
  return success;
}

// * iterate 내부 함수
static void iterate_reset_donate(struct list_elem *elem, void *aux) {
  struct thread *curr = thread_current();
  struct thread *next = (struct thread *)aux;
  struct thread *t = list_entry(elem, struct thread, elem);

  if (t->priority > curr->priority) {
    curr->donate_list[t->priority]--;
  }

  for (int i = PRI_MAX; i >= PRI_MIN; i--) {
    if (t->donate_list[i] > 0 && i > curr->priority) {
      curr->donate_list[i]--;
    }
  }

  t->holder = next;
}

// * lock 해제
void lock_release(struct lock *lock) {
  ASSERT(lock != NULL);
  ASSERT(lock_held_by_current_thread(lock));

  if (!thread_mlfqs) {
    struct list_elem *next_elem;
    struct thread *next;

    if (!list_empty(&lock->semaphore.waiters)) {
      struct list_elem *curr;
      struct thread *donator;

      /* unblock할 애 */
      next_elem = list_min(&lock->semaphore.waiters, high_prio, NULL);
      next = thread_entry(next_elem);

      /* list를 돌면서 lock donation을 reset */
      list_iterate(&lock->semaphore.waiters, iterate_reset_donate, next);

      thread_current()->holder = NULL;
    }
  }

  lock->holder = NULL;
  sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock) {
  ASSERT(lock != NULL);

  return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem {
  struct list_elem elem;      /* List element. */
  struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it.

   조건 변수 COND를 초기화합니다. 조건 변수를 사용하면 코드의 한 부분이 조건을
   신호로 보낼 수 있고, 협력하는 코드가 그 신호를 받아 그것에 대응하여 작동할
   수 있습니다. */
void cond_init(struct condition *cond) {
  ASSERT(cond != NULL);

  list_init(&cond->waiters);  // 세마포어 배열을 가짐
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep.

   이 함수는 LOCK을 원자적으로 해제하고, 다른 코드 부분에서 COND가
   시그널되기를 기다립니다. COND가 시그널되면 반환하기 전에 LOCK이 다시
   획득됩니다. 이 함수를 호출하기 전에 LOCK이 획득되어 있어야 합니다.

   이 함수에 의해 구현된 모니터는 "Hoare" 스타일이 아닌 "Mesa" 스타일입니다,
   즉, 신호를 보내고 받는 것은 원자적인 연산이 아닙니다. 따라서 대기가 완료된
   후에 일반적으로 호출자는 조건을 다시 확인하고 필요한 경우 다시 기다려야
   합니다.

   주어진 조건 변수는 단 하나의 잠금과만 연관되어 있지만, 하나의 잠금은 여러
   조건 변수와 연관될 수 있습니다. 즉, 잠금에서 조건 변수로의 매핑은 일대다
   관계입니다.

   이 함수는 잠들 수 있으므로, 인터럽트 핸들러 내에서 호출되어서는 안됩니다.
   이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, 잠들어야 할 경우
   인터럽트는 다시 활성화됩니다.*/
void cond_wait(struct condition *cond, struct lock *lock) {
  struct semaphore_elem waiter;

  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  sema_init(&waiter.semaphore, 0);               // 세마포어 초기화
  list_push_back(&cond->waiters, &waiter.elem);  // 세마포어를 리스트에 넣는다
  lock_release(lock);                            // 락을 푼다
  sema_down(&waiter.semaphore);                  // 여기서 못들어감!!
  lock_acquire(lock);                            // 락을 얻으려고함
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.

   COND(LOCK에 의해 보호됨)를 기다리는 스레드가 있으면, 이 함수는 그 중
   하나에게 대기 상태에서 깨어나도록 신호를 보냅니다. 이 함수를 호출하기 전에
   LOCK이 획득되어 있어야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로, 인터럽트 핸들러 내에서 조건
   변수에 신호를 보내려고 시도하는 것은 의미가 없습니다. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED) {
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  // 리스트 중 특정 세마포어를 up함
  struct list_elem *wake_elem;
  struct semaphore_elem *wake;
  if (!list_empty(&cond->waiters)) {
    wake_elem = list_min(&cond->waiters, high_sema, NULL);
    wake = list_entry(wake_elem, struct semaphore_elem, elem);
    list_remove(wake_elem);
    sema_up(&wake->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.

   COND(LOCK에 의해 보호됨)를 기다리는 모든 스레드(있는 경우)를 깨웁니다. 이
   함수를 호출하기 전에 LOCK이 획득되어 있어야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로, 인터럽트 핸들러 내에서 조건
   변수에 신호를 보내려고 시도하는 것은 의미가 없습니다. */
void cond_broadcast(struct condition *cond, struct lock *lock) {
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);

  while (!list_empty(&cond->waiters)) cond_signal(cond, lock);
}

/* waiters of semaphore cmp */
bool high_sema(struct list_elem *_a, struct list_elem *_b, void *aux) {
  struct semaphore_elem *a = list_entry(_a, struct semaphore_elem, elem);
  struct semaphore_elem *b = list_entry(_b, struct semaphore_elem, elem);

  int a_prio = 0;
  int b_prio = 0;

  struct thread *a_th;
  struct thread *b_th;

  a_th = thread_entry(list_min(&a->semaphore.waiters, high_prio, NULL));
  b_th = thread_entry(list_min(&b->semaphore.waiters, high_prio, NULL));

  return get_priority(a_th) > get_priority(b_th);
}