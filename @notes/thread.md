# [PintOS] `threads`를 뜯어보자

폴더구조
```c
threads
│       ├── init.d
│       ├── interrupt.d
│       ├── intr-stubs.d
│       ├── kernel.lds.s
│       ├── malloc.d
│       ├── mmu.d
│       ├── palloc.d
│       ├── start.d
│       ├── synch.d
│       ├── thread.d
```

## assembly code 동작
### 01) loader.S
### 02) start.S


## PintOS 스레드는 어떻게 돌아갈까?
## PinoOS 기준
`thread_init`을 하면 main thread(`initial_thread`)가 만들어지고 실행된다. 스케줄링을 위해 `thread_start`를 실행하면, `idle_thread`를 만들고 돌린다. `idle_thread`는 인터럽트를 비활성화하고 자기 자신을 블락한 다음, `sti; hlt;` 명령어를 실행하여 인터럽트를 활성화하고, 인터럽트가 발생할 때까지 CPU를 대기 상태로 전환하는 일을 반복한다. `create_thread`는 스레드 이름, 우선순위를 인자로 받아 스레드를 초기화(`init_thread`)하고, 스레드 내부 `intr_frame` 구조체에 인자로 받은 실행 함수 (`thread_func` type)와 그 인자를 저장한 후 `thread_unblock`을 통해 status를 `THREAD_READY`로 바꾼 후 `ready_list에` 넣는다. 그러면 인터럽트마다 실행되는 함수인 `thread_tick`의 `if(thread_tick >= TIME_SLICE) intr_yield_on_return();` 라인에 의해 `thread_yield`가 일어날 때 `ready_list`에서 꺼내어져서, `thread_launch`와 `do_iret` 함수에 의해 실질적인 context switching이 발생하여 실행되게 된다.

그러면 인터럽트는 누가 보내는 걸까? Programmable Interval Timer (PIT)인 8254 타이머가 보낸다. 이 타이머는 컴퓨터의 메인보드에 위치하여, `init_timer`에서 세팅된 인터벌마다 PIC (Programmable Interrupt Controller, 전통적으로는 8259A)에 인터럽트를 발생시켜야 한다는 요청을 전달한다. 구체적으로는 IRQ0, Interrupt Request 0 라인을 통해 master PIC에 요청한다. (여기서 잘 모르겠다. 하드웨어적인 과정인 것 같다..) x86아키텍처에서 IRQ의 값은 벡터값 0x08로 매핑되어 있다. 그런데 이것은 internal interrupt (CPU 예외)의 벡터값과 중복되기 때문에, PIC에서 0x08 벡터를 0x20으로 조정하여 CPU에 전달한다. *CPU는 받아온 벡터값을 Interrupt Handler Table에서 찾아 이에 대응하는 interreupt handler (우리의 경우에는 timer_interrupt)를 실행시킨다.* 인터럽트를 핸들링하는 것은 CPU다. 이 함수가 `intr_handler`이며, intr-stubs.S의 가장 위 코드를 보면 중간에 `call intr_handler` 라인을 확인할 수 있다. `intr_handler`는 항상 인터럽트를 비활성화하고 시작하는데, 이는 일반적으로 하나의 인터럽트 핸들링은 다른 인터럽트에 의해 중단될 수 없기 때문이다. 인터럽트가 종료된 후에는, `call intr_handler`하기 전 stack에 push해두었던 다음 인스트럭션 주소를 pop해와서 %rip를 해당 인스트럭션의 주소로 바꾼다.

**👀 `intr-stub.S`를 읽어보자**
```as
#define zero pushq $0;  // error code 0을 스택에 푸시한다
#define REAL            // cpu가 하는 동작

.section .data
.globl intr_stubs
intr_stubs:

/* STUB 매크로를 정의한다 */
#define STUB(NUMBER, TYPE)
.section .text;                 // 데이터 섹션에 명령어를 위치시켜라
.globl intr##NUMBER##_stub;     // intr00_stub과 같은 전역 함수 선언
.func intr##NUMBER##_stub;      // 함수 정의 시작
intr##NUMBER##_stub:
	TYPE;                       // type (zero인 경우 pushq $0)  
	push $0x##NUMBER;           // $0x00과 같은 vec_no push
	jmp intr_entry;             // intr_entry 로 jump
.endfunc;
.section .data;                 // 데이터 섹션에
.quad intr##NUMBER##_stub;      // intr00_stub의 주소를 8바이트로 저장

/* 하나의 인터럽트 핸들러를 저장하는 STUB매크로 */
STUB(00, zero) STUB(01, zero) STUB(02, zero) ... 256
```

**👀 IRQ란 뭔가?**<br/>
모르겠다.

**👀 PIC에서 왜 벡터값을 재매핑해야 할까?**<br/>
애초에 설계 단계에서 잘 분리해뒀으면 안되는 걸까? 하지만 컴퓨터 아키텍처는 역사적으로 발전되어 왔으며, 시스템의 복잡성이 증가함에 따라 처리해야 할 인터럽트의 수가 점차 증가하기도 했다. 이에 따라 벡터값을 새로 다 할당하기보다는 소프트웨어적으로 재매핑하여 처리할 필요성이 생겨났다. 아키텍처란 역사적 산출물이므로.. 어느 한 순간에 완벽한 설계에 의해 탄생한 것은 아니다.


## `threads/` 코드 분석

### 00) `thread.h` 헤더파일
헤더파일부터 살펴보면, 스레드 우선순위와 `tid_t`, 함수 선언, 가장 중요하게는 스레드 구조체가 선언되어 있다.

```c
struct thread {
  tid_t tid;                  /* tid */
  enum thread_status status;  /* 상태 */
  char name[16];              /* 이름 */

  int priority;              /* 실행 우선순위 */
  struct list_elem elem;     /* 리스트 관리 */
  
  ...

  struct intr_frame tf;      /* 인터럽트 프레임 */
  unsigned magic;            /* 스택오버플로 체크 */
};

// interrupt.h
struct intr_frame {

}
```


### 01) `thread_init`
스레드를 사용하기 위한 환경 설정을 해주는 함수이다.
1. 전역 디스크립터 테이블, 스레드 락, 레디큐, 삭제큐를 초기화한다.
    * Global Descriptor Table (GDT)
    * tid_lock 초기화: 스레드가 각각의 개별적인 id를 갖도록 lock을 걸어 관리한다.
    * list_init: 스레드 스케줄링 및 관리에 사용할 doubly linked list 초기화
* running_thread()는 현재 rsp를 읽어서 페이지의 시작지점으로 가서, 스레드 주소를 읽어옴
  * 이게 왜 가능? 페이지 구조가 그렇게 되어있나?
* 메인 스레드를 하나 만들어서
* running_thread()의 위치에 메인 스레드를 올림
* *initial_thread = running_thread()의 status와 tid을 지정
  * status는 THREAD_RUNNING
  * tid는 allocate_tid로 받아옴


### 02) `init_thread`
개별 스레드의 환경설정
* 인자로 받은 thread *t에 memset으로 하나의 스레드를 만든다
* 인자로 받은 *name, priority를 설정

### 03) `thread_start`
* idle thread를 만들고 대기큐에 넣음
* idle thread는 따로 설정을 해줘야함. 그걸 기다려줌.. 왜?

### 04) `thread_tick`
* 스레드 사용 환경의 tick을 잼
* kernel_thread, idle_thread, thread의 tick을 ++

### 05) `thread_create`
* 주소공간으로부터 페이지 하나를 할당받음
* 이름, 우선순위, 실행함수, 인자를 받아 스레드를 만듦
* 페이지 프레임 설정 
  * rip = 
  * rdi = 
  * rsi =
* 스레드를 대기상태로 전환 (unblock)

### 06) `thread_block`
* 현재상태를 블락으로 변경
* 대기큐의 front를 뽑아와 실행
* 🔥 다시 대기큐에 넣는 부분이 없음. 따라서 unblock할때까지 스케줄링 안됨
* 이 함수를 부르는 쪽 입장에서는, 일정 시간만 블락하고자 한다면 루프를 돌려 시간을 재야 함 (unblock을 해줘야 하므로)

### 07) `thread_exit`
* 스레드의 상태를 THREAD_DYING으로 변경

### 08) `thread_yield`
* 현재 스레드를 대기상태로 변경
* 다음 스레드를 불러와 실행

### 09) `idle`
* `idle` 스레드에서 실행되는 함수
* ?? interruption 비활성화, 현재 스레드 블락 (다음거 실행) <- 무한루프

### 10) `kernel_thread`
* 

### 11) `do_schecule`
* 삭제할 스레드 정리하여 페이지 프리해준다 by destruction queue
* 현재 스레드의 상태를 인자로 주어진 상태로 갱신한다.
* schedule 실행

### 12) `schedule`
* 대기큐에서 다음 스레드를 가져온다.
* 다음 스레드의 상태를 갱신한다
* 현재 스레드가 DYING이면 destruction req로 넣는다 (실행을 마치면 DYING으로 바꾸는 부분은 어디서?)
* 다음 스레드를 런치한다.

### 13) `do_iret`

### 14) `thread_launch`

### 15) `next_thread_to_run`

### 16) `thread_current`