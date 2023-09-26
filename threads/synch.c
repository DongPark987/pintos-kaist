/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below.

   이 파일은 Nachos 교육용 운영 체제의 소스 코드에서 파생되었습니다.
   Nachos 저작권 고지가 아래에 전문으로 재생산되었습니다. */

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

   저작권 (c) 1992-1996 캘리포니아 대학령의 모든 권리 보유.

   이 소프트웨어와 그 문서를 별도의 비용 없이 사용, 복사, 수정 및 배포하는 허가가 여기에 부여됩니다.
   서면 합의 없이, 다음의 저작권 고지와 다음 두 단락이 이 소프트웨어의 모든 사본에 나타나야 합니다.

   캘리포니아 대학령은 이 소프트웨어와 그 문서의 사용으로 인해 발생하는 직접, 간접, 특별, 우발적 또는
   결과적 손해에 대해 어떤 당사자에게도 책임을 지지 않습니다. 심지어 캘리포니아 대학령이 그러한 손해
   가능성을 미리 경고했더라도.

   캘리포니아 대학령은 명시적 또는 특정 목적에 대한 상업성 및 적합성을 포함하되 이에 한정되지 않는
   어떠한 보증도 명시적으로 부인합니다. 여기에서 제공하는 소프트웨어는 "있는 그대로" 제공되며,
   캘리포니아 대학령은 유지 보수, 지원, 업데이트, 개선 또는 수정을 제공할 의무가 없습니다.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any).

   세마포어(SEMA)를 VALUE로 초기화합니다. 세마포어는 음수가 아닌 정수와
   이를 조작하는 데 사용되는 두 가지 원자 연산을 가지고 있습니다:

   - down 또는 "P": 값이 양수가 될 때까지 기다린 다음 값을 감소시킵니다.

   - up 또는 "V": 값을 증가시킵니다 (그리고 대기 중인 스레드 중 하나를 깨우며,
   대기 중인 스레드가 있는 경우).
    */
void sema_init(struct semaphore *sema, unsigned value)
{
   /*sema 인자가 NULL값이면 ASSERT 발생*/
   ASSERT(sema != NULL);
   /*세마포어 value 인자로 받은 value 값을 대입*/
   sema->value = value;
   /*list.h에 구현되어 있는 연결 리스트 사용*/
   list_init(&sema->waiters);
}

// TODO:
/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function.

   세마포어(SEMA)에 대한 다운 또는 "P" 연산입니다.
   SEMA의 값이 양수가 될 때까지 기다린 다음 원자적으로 값을 감소시킵니다.

   이 함수는 슬립할 수 있으므로 인터럽트 처리기 내에서 호출해서는 안 됩니다.
   이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만,
   만약 이 함수가 슬립하면 다음 예정된 스레드가 아마도 인터럽트를 다시 활성화할 것입니다.
   이것이 "sema_down" 함수입니다. */
void sema_down(struct semaphore *sema)
{
   enum intr_level old_level;

   ASSERT(sema != NULL);
   ASSERT(!intr_context());

   old_level = intr_disable();
   while (sema->value == 0)
   {
      list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL);
      thread_block();
   }
   sema->value--;
   intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler.
   세마포어에 대한 "다운" 또는 "P" 연산을 수행합니다.
   다만, 세마포어가 이미 0인 경우에만 수행합니다. 세마포어가 감소되면
   true를 반환하고, 그렇지 않으면 false를 반환합니다.
   이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
bool sema_try_down(struct semaphore *sema)
{
   enum intr_level old_level;
   bool success;

   ASSERT(sema != NULL);

   old_level = intr_disable();
   if (sema->value > 0)
   {
      sema->value--;
      success = true;
   }
   else
      success = false;
   intr_set_level(old_level);

   return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
   enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
   sema->value++;
	intr_set_level (old_level);
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
   struct semaphore sema[2];
   int i;

   printf("Testing semaphores...");
   sema_init(&sema[0], 0);
   sema_init(&sema[1], 0);
   thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
   for (i = 0; i < 10; i++)
   {
      sema_up(&sema[0]);
      sema_down(&sema[1]);
   }
   printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
   struct semaphore *sema = sema_;
   int i;

   for (i = 0; i < 10; i++)
   {
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
   instead of a lock. */
void lock_init(struct lock *lock)
{
   ASSERT(lock != NULL);

   lock->holder = NULL;
   sema_init(&lock->semaphore, 1);
}

// TODO:
/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(!lock_held_by_current_thread(lock));

   struct thread *holder;
   struct thread *donator;
   int before_priority;

   if (lock->holder != NULL)
   {
      // printf("\n========== lock_acquire에서 priority 변경 from: %s %d", lock->holder->name, lock->holder->priority);
      // printf("\n=====lock holder: %s / priority: %d /", lock->holder->name, lock->holder->priority);

      // 만약 이미 다른 스레드가 락을 가지고 있다면 현재 스레드가 기다리는 락으로 현재 락을 추가
      thread_current()->wait_on_lock = lock;

      // Donate
      donator = thread_current();
      holder = lock->holder;
      while (holder != NULL)
      {
         // printf("========🥹나야나\n");
         if (holder->priority < donator->priority)
         {
            holder->priority = donator->priority;
            list_push_back(&holder->donators, &donator->d_elem);
            // list_insert_ordered(&holder->donators, &donator->d_elem, cmp_priority, NULL);
         }

         donator = holder;
         holder = (holder->wait_on_lock != NULL) ? holder->wait_on_lock->holder : NULL;
      }
      // printf("=========== 🚀 탈출\n");
      // printf("🌟 size: %d\n", list_size(&lock->holder->donators));
      // printf("👉🏻 lock holder: %s / priority: %d\n", thread_current()->name, lock->holder->priority);
   }

   sema_down(&lock->semaphore);

   lock->holder = thread_current();
   // printf("\n➡️ lock holder: %s / priority: %d\n", lock->holder->name, lock->holder->priority);
   thread_yield();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
   bool success;

   ASSERT(lock != NULL);
   ASSERT(!lock_held_by_current_thread(lock));

   success = sema_try_down(&lock->semaphore);
   if (success)
      lock->holder = thread_current();
   return success;
}

// TODO:
/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(lock_held_by_current_thread(lock));

   sema_up(&lock->semaphore); // waiters 리스트의 가장 앞에 있는 스레드를 unblock해서 ready 상태로 만들고, sema value를 1 증가시키기

   // donators에서 현재 락을 wait_on_lock으로 가지고 있는 스레드들을 찾아서 제거
   struct list_elem *e;
   if (lock->holder != NULL && !list_empty(&lock->holder->donators))
   {
      for (e = list_begin(&thread_current()->donators); e != list_end(&thread_current()->donators); e = list_next(e))
         if (list_entry(e, struct thread, d_elem)->wait_on_lock == lock)
         {
            list_remove(e);
         }
   }

   lock->holder->priority = (!list_empty(&lock->holder->donators)) ? list_entry(list_max(&lock->holder->donators, cmp_priority, NULL), struct thread, d_elem)->priority : lock->holder->origin_priority;
   lock->holder = NULL;
   thread_yield();
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
   ASSERT(lock != NULL);

   return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem
{
   struct list_elem elem;      /* List element. */
   struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
   ASSERT(cond != NULL);

   list_init(&cond->waiters);
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
   we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock)
{
   struct semaphore_elem waiter;

   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   sema_init(&waiter.semaphore, 0);
   list_push_back(&cond->waiters, &waiter.elem);
   lock_release(lock);
   sema_down(&waiter.semaphore);
   lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   if (!list_empty(&cond->waiters))
      sema_up(&list_entry(list_pop_front(&cond->waiters),
                          struct semaphore_elem, elem)
                   ->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);

   while (!list_empty(&cond->waiters))
      cond_signal(cond, lock);
}
