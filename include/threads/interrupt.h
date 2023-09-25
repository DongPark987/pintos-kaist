#ifndef THREADS_INTERRUPT_H
#define THREADS_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

/* Interrupts on or off? */
enum intr_level {
  INTR_OFF, /* Interrupts disabled. */
  INTR_ON   /* Interrupts enabled. */
};

enum intr_level intr_get_level(void);
enum intr_level intr_set_level(enum intr_level);
enum intr_level intr_enable(void);
enum intr_level intr_disable(void);

/* Interrupt stack frame. */
struct gp_registers { /* 8byte * 16 */
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t rbp;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
} __attribute__((packed));

/* [Interrupt Frame]
 * 인터럽트를 처리하기 위해 현재의 실행 상태를 저장한다.
 * 인터럽트 처리가 끝나면 얘를 빼내와서 다시 실행한다.
 * rax-r15 레지스터값(R), 프로그램 카운터(rip), 스택포인터(rsp), 베이스
 * 포인터(rbs), EFLAG 상태 플래그(eflags), 코드/데이터 세그먼트 주소 (cs, ds),
 * 오류코드 등을 인터럽트 스택에 저장한다.
 */
struct intr_frame {
  /* Pushed by intr_entry in intr-stubs.S.
     These are the interrupted task's saved registers. */
  struct gp_registers R; /* rax to r15 (64bit registers) */
  uint16_t es;           /* segment register */
  uint16_t __pad1;       /* padding */
  uint32_t __pad2;       /* padding */
  uint16_t ds;           /* data segment */
  uint16_t __pad3;
  uint32_t __pad4;

  /* Pushed by intrNN_stub in intr-stubs.S. */
  uint64_t vec_no; /* Interrupt vector number.
                      인터럽트 벡터값, 이걸로 인터럽트 핸들러를 찾음 */
                   /* Sometimes pushed by the CPU,
                      otherwise for consistency pushed as 0 by intrNN_stub.
                      진짜 CPU가 푸시할수도 있고 intrNN_stub이 할수도 있음 (Kbd같은?)
                      The CPU puts it just under `eip', but we move it here. */
  uint64_t error_code;

  /* Pushed by the CPU.
     These are the interrupted task's saved registers.
     인터럽트된 작업의 저장된 레지스터값들 */
  uintptr_t rip;   /* Instruction Pointer */
  uint16_t cs;     /* Code Segment */
  uint16_t __pad5; /* padding */
  uint32_t __pad6; /* padding */
  uint64_t eflags; /* EFLAG: CF, PF, AF, ZF, SF, TF, IF, DF, OF */
  uintptr_t rsp;   /* Stack Pointer */
  uint16_t ss;     /* Segment Register */
  uint16_t __pad7;
  uint32_t __pad8;
} __attribute__((packed));

/* interrupt handler는 프레임을 받아서 어디다 두지 */
typedef void intr_handler_func(struct intr_frame *);

void intr_init(void);
void intr_register_ext(uint8_t vec, intr_handler_func *, const char *name);
void intr_register_int(uint8_t vec, int dpl, enum intr_level,
                       intr_handler_func *, const char *name);
bool intr_context(void);
void intr_yield_on_return(void);

void intr_dump_frame(const struct intr_frame *);
const char *intr_name(uint8_t vec);

#endif /* threads/interrupt.h */
