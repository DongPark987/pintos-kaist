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
   캘리포니아 대학령은 유지 보수, 지원, 업데이트, 개선 또는 수정을 제공할 의무가 없습니다. */

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
      list_push_back(&sema->waiters, &thread_current()->elem);
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

bool cmp_priority_max(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
   const struct thread *a = list_entry(a_, struct thread, elem);
   const struct thread *b = list_entry(b_, struct thread, elem);
   return (thread_get_priority_manual(a) < thread_get_priority_manual(b));
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler.
   "세마포어(SEMA)"에 대한 "Up" 또는 "V" 연산. SEMA의 값을 증가시키고
   SEMA를 기다리는 스레드 중 하나를 깨웁니다(만약 대기 중인 스레드가 있다면).
   이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */

void sema_up(struct semaphore *sema)
{
   enum intr_level old_level;
   struct thread *max_t = NULL;
   struct thread *curr = thread_current();
   struct list_elem *max_elem;

   ASSERT(sema != NULL);

   old_level = intr_disable();

   if (!list_empty(&sema->waiters))
   {
      max_elem = list_max((&sema->waiters), cmp_priority_max, NULL);
      max_t = list_entry(max_elem, struct thread, elem);
      list_remove(max_elem);
      thread_unblock(max_t);
   }

   sema->value++;
   if (max_t != NULL && !intr_context() && thread_get_priority() < thread_get_priority_manual(max_t))
      thread_yield();
   intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on.

   "셈포어에 대한 자체 테스트를 통해 두 개의 스레드 간에 '핑-퐁'
    제어를 만들어 봅니다. printf() 호출을 삽입하여 현재 상황을 확인해보세요."*/
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
   instead of a lock.

   "LOCK를 초기화합니다. 잠금은 언제나 최대 하나의 스레드만 보유할 수 있습니다.
    저희의 잠금은 '재귀적'이지 않습니다. 즉, 현재 잠금을 보유하고 있는 스레드가
    해당 잠금을 다시 얻으려고 시도하는 것은 오류입니다.

    잠금은 초기값이 1인 세마포어의 특수화로, 잠금과 이러한 세마포어 간의 차이점은 두 가지입니다.
    첫째, 세마포어는 1보다 큰 값을 가질 수 있지만 잠금은 한 번에 하나의 스레드만 소유할 수 있습니다.
    둘째, 세마포어는 소유자가 없으며, 따라서 하나의 스레드가 세마포어를 '다운'하고 다른 스레드가
    '업'할 수 있지만 잠금의 경우 동일한 스레드가 잠금을 획득하고 해제해야 합니다.
    이러한 제한이 무거워질 때, 세마포어 대신 잠금을 사용해야 하는 좋은 신호입니다."*/
void lock_init(struct lock *lock)
{
   ASSERT(lock != NULL);
   lock->holder = NULL;
   for (int i = 0; i < 64; i++)
      lock->donation_list[i] = 0;
   sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep.

   LOCK를 획득하고, 필요한 경우 사용 가능할 때까지 대기합니다.
   현재 스레드가 이미 잠금을 보유하고 있으면 안 됩니다.

   이 함수는 슬립할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다.
   이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만 슬립이 필요한 경우 인터럽트가 다시 활성화됩니다. */
void lock_acquire(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(!lock_held_by_current_thread(lock));
   struct thread *curr_t = thread_current();
   struct thread *max_waiter_t;
   struct list_elem *max_waiter_elem;

   if (thread_mlfqs)
   {
      sema_down(&lock->semaphore);
      curr_t->wait_on_lock = NULL;
      lock->holder = curr_t;
      return;
   }

   if (lock->holder != NULL)
   {
      curr_t->wait_on_lock = lock;

      int curr_priority = thread_get_priority();
      struct lock *next_lock = lock;
      while (next_lock != NULL)
      {
         if (next_lock->holder->priority < curr_priority)
         {
            next_lock->holder->donation_cnt++;
            next_lock->holder->donation_list[curr_priority]++;
            next_lock->donation_list[curr_priority]++;
            if (next_lock->holder->status == THREAD_READY)
               thread_relocate_ready(next_lock->holder);
         }
         next_lock = next_lock->holder->wait_on_lock;
      }
   }
   sema_down(&lock->semaphore);
   curr_t->wait_on_lock = NULL;
   lock->holder = curr_t;
   if (!list_empty(&lock->semaphore.waiters))
   {
      max_waiter_elem = list_max((&lock->semaphore.waiters), cmp_priority_max, NULL);
      max_waiter_t = list_entry(max_waiter_elem, struct thread, elem);
      int max_priority = thread_get_priority_manual(max_waiter_t);
      if (curr_t->priority < max_priority)
      {
         curr_t->donation_cnt++;
         curr_t->donation_list[max_priority]++;
         lock->donation_list[max_priority]++;
      }
   }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler.

   LOCK를 획득하려 시도하고 성공하면 true를 반환하며 실패하면 false를 반환합니다.
   현재 스레드가 이미 잠금을 보유하고 있으면 안 됩니다.

    이 함수는 슬립하지 않으므로 인터럽트 핸들러 내에서 호출할 수 있습니다. */
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

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler.

   현재 스레드가 소유해야 하는 LOCK을 해제합니다.
   이것은 lock_release 함수입니다.

    인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서 잠금을
    해제하려고 시도하는 것은 의미가 없습니다. */
void lock_release(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(lock_held_by_current_thread(lock));
   struct thread *t = thread_current();
   if (!thread_mlfqs)
   {
      for (int i = 0; i < 63; i++)
      {
         t->donation_list[i] -= lock->donation_list[i];
         t->donation_cnt -= lock->donation_list[i];
         lock->donation_list[i] = 0;
      }
   }
   lock->holder = NULL;
   sema_up(&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.)
   "현재 스레드가 LOCK을 보유하고 있는 경우 true를 반환하고, 그렇지 않은 경우 false를 반환합니다.
   (다른 스레드가 잠금을 보유하는지 테스트하는 것은 경합 조건이 발생할 수 있으므로 주의해야 합니다.)" */
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
   code to receive the signal and act upon it.

   "조건 변수 COND를 초기화합니다. 조건 변수는 하나의 코드 조각이 조건을
   신호로 보내고 협력 코드가 해당 신호를 받아 처리할 수 있게 합니다." */
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
   we need to sleep.

   "원자적으로 LOCK을 해제하고 다른 코드 조각에 의해 COND가 신호될 때까지 대기합니다.
   COND가 신호되면 반환하기 전에 LOCK을 다시 획득합니다. 이 함수를 호출하기 전에 반드시
   LOCK이 보유되어 있어야 합니다.

   이 함수로 구현된 모니터는 'Mesa' 스타일이며 'Hoare' 스타일이 아닙니다.
   즉, 신호를 보내고 받는 것은 원자적인 작업이 아닙니다.
   따라서 대기가 완료된 후에 조건을 다시 확인하고 필요한 경우 다시 대기해야 하는 경우가 일반적입니다.

   특정 조건 변수는 하나의 잠금과 연관되지만 하나의 잠금은 여러 개의 조건 변수와 연관될 수 있습니다.
   즉, 잠금에서 조건 변수로의 일대다 매핑이 있습니다.

   이 함수는 슬립할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다.
   이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만 슬립이 필요한 경우 인터럽트가 다시 활성화됩니다." */

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

bool cmp_cond_max(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
   const struct semaphore *s1 = &list_entry(a_, struct semaphore_elem, elem)->semaphore;
   const struct semaphore *s2 = &list_entry(b_, struct semaphore_elem, elem)->semaphore;
   return (thread_get_priority_manual(list_entry(list_front(&s1->waiters), struct thread, elem)) < thread_get_priority_manual(list_entry(list_front(&s2->waiters), struct thread, elem)));
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.

   "만약 COND에서 (LOCK로 보호되는) 대기 중인 스레드가 있다면,
   이 함수는 그 중 하나에게 대기에서 깨어나라는 신호를 보냅니다.
   이 함수를 호출하기 전에 반드시 LOCK이 보유되어 있어야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서
   조건 변수를 신호하는 것은 의미가 없습니다." */
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
   if (thread_mlfqs)
   {
   }
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   if (!list_empty(&cond->waiters))
   {
      struct list_elem *tmp_elem = list_max(&cond->waiters, cmp_cond_max, NULL);
      list_remove(tmp_elem);
      sema_up(&list_entry(tmp_elem, struct semaphore_elem, elem)->semaphore);
   }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.

   "만약 COND에서 (LOCK로 보호되는) 대기 중인 스레드가 있다면,
   이 함수는 모든 스레드를 깨웁니다.
   이 함수를 호출하기 전에 반드시 LOCK이 보유되어 있어야 합니다.

   인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러
   내에서 조건 변수를 신호하는 것은 의미가 없습니다." */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);

   while (!list_empty(&cond->waiters))
      cond_signal(cond, lock);
}