함수정리


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
* 전역 디스크립터 테이블, 스레드 락, 레디큐, 삭제큐를 초기화 (구체적으로 어떤일?)
* running_thread()는 현재 rsp를 읽어서 페이지의 시작지점으로 가서, 스레드 주소를 읽어옴
  * 이게 왜 가능? 페이지 구조가 그렇게 되어있나?
* 메인 스레드를 하나 만들어서
* running_thread()의 위치에 메인 스레드를 올림
* *initial_thread = running_thread()의 status와 tid을 지정
  * status는 THREAD_RUNNING
  * tid는 allocate_tid로 받아옴

### `init_thread`
* 인자로 받은 thread *t에 memset으로 하나의 스레드를 만든다
* 인자로 받은 *name, priority를 설정



연관함수들
* `list_init`: 이중연결리스트로 초기화
* `lock_init`: 이진 세마포어 락을 1로 설정
* 