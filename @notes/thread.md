함수정리

* 락 거는 거: 스레드의 
* 레지스터값에 저장하는거
* gdt 저장되는 거
```c
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
```

### `thread_init`
스레드를 사용하기 위한 환경 설정
* 전역 디스크립터 테이블, 스레드 락, 레디큐, 삭제큐를 초기화 (구체적으로 어떤일?)
* running_thread()는 현재 rsp를 읽어서 페이지의 시작지점으로 가서, 스레드 주소를 읽어옴
  * 이게 왜 가능? 페이지 구조가 그렇게 되어있나?
* 메인 스레드를 하나 만들어서
* running_thread()의 위치에 메인 스레드를 올림
* *initial_thread = running_thread()의 status와 tid을 지정
  * status는 THREAD_RUNNING
  * tid는 allocate_tid로 받아옴


### `init_thread`
개별 스레드의 환경설정
* 인자로 받은 thread *t에 memset으로 하나의 스레드를 만든다
* 인자로 받은 *name, priority를 설정

### `thread_start`
* idle thread를 만들고 대기큐에 넣음
* idle thread는 따로 설정을 해줘야함. 그걸 기다려줌.. 왜?

## `thread_tick`
* 스레드 사용 환경의 tick을 잼
* kernel_thread, idle_thread, thread의 tick을 ++

## `thread_create`
* 주소공간으로부터 페이지 하나를 할당받음
* 이름, 우선순위, 실행함수, 인자를 받아 스레드를 만듦
* 페이지 프레임 설정 
  * rip = 
  * rdi = 
  * rsi =
* 스레드를 대기상태로 전환 (unblock)

## `thread_block`
* 현재상태를 블락으로 변경
* 대기큐의 front를 뽑아와 실행
* 🔥 다시 대기큐에 넣는 부분이 없음. 따라서 unblock할때까지 스케줄링 안됨
* 이 함수를 부르는 쪽 입장에서는, 일정 시간만 블락하고자 한다면 루프를 돌려 시간을 재야 함 (unblock을 해줘야 하므로)

## `thread_exit`
* 스레드의 상태를 THREAD_DYING으로 변경

## `thread_yield`
* 현재 스레드를 대기상태로 변경
* 다음 스레드를 불러와 실행

## `idle`
* `idle` 스레드에서 실행되는 함수
* ?? interruption 비활성화, 현재 스레드 블락 (다음거 실행) <- 무한루프

## `kernel_thread`
* 

## `do_schecule`
* 삭제할 스레드 정리하여 페이지 프리해준다 by destruction queue
* 현재 스레드의 상태를 인자로 주어진 상태로 갱신한다.
* schedule 실행

## `schedule`
* 대기큐에서 다음 스레드를 가져온다.
* 다음 스레드의 상태를 갱신한다
* 현재 스레드가 DYING이면 destruction req로 넣는다 (실행을 마치면 DYING으로 바꾸는 부분은 어디서?)
* 다음 스레드를 런치한다.

## `do_iret`

## `thread_launch`

### 기타 함수들
* `thread_name`: 스레드 이름 반환
* `print_thread_stat`: 현재 tick 출력
* `thread_current`: 현재 running thread 반환 (rtstack의 시작페이지 주소)
* `thread_tid`: 스레드 tid 반환
* `next_thread_to_run`: 대기큐에서 스레드 뽑아옴. 없으면 idle
