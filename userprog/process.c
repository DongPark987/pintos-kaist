#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/synch.h"

#define VM
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup(void);
static bool load(const char *file_name, struct intr_frame *if_);
static void initd(void *f_name);
static void __do_fork(void *);

struct semaphore exec_sema;
struct semaphore fdt_cpy_sema;

struct intr_frame tmp_if_;

/* General process initializer for initd and other process. */
static void
process_init(void)
{
	struct thread *current = thread_current();
	current->fdt = palloc_get_multiple(PAL_ZERO, 3);
	if (current->fdt == NULL)
		return;
	current->fdt[0].stdio = 1;
	current->fdt[1].stdio = 2;
	current->fd_cnt = 0;
	current->is_kernel = 0;
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */

/* 첫 번째 사용자 영역 프로그램 "initd"를 시작합니다. 이 프로그램은 FILE_NAME에서 로드됩니다.
 * 새로운 스레드는 process_create_initd()가 반환되기 전에 스케줄될 수 있습니다 (심지어 종료될 수도 있음).
 * initd의 스레드 ID를 반환하거나 스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다.
 * 이 함수는 한 번만 호출되어야 합니다. */
tid_t process_create_initd(const char *file_name)
{
	char *fn_copy;
	tid_t tid;
	char *save_ptr;
	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page(PAL_ZERO);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy(fn_copy, file_name, PGSIZE);

	/* Create a new thread to execute FILE_NAME. */
	sema_init(&exec_sema, 1);
	// printf("초기화 끝\n");

	tid = thread_create(strtok_r(file_name, " ", save_ptr), PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page(fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd(void *f_name)
{

#ifdef VM
	supplemental_page_table_init(&thread_current()->spt);
#endif

	process_init();

	if (process_exec(f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */

/* 현재 프로세스를 'name'으로 복제합니다. 새로운 프로세스의 스레드 ID를 반환하거나
 * 스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다. */
tid_t process_fork(const char *name, struct intr_frame *if_ UNUSED)
{
	/* Clone current thread to new thread.*/
	struct thread *curr = thread_current();
	curr->fork_tf = palloc_get_page(PAL_ZERO);
	memcpy(curr->fork_tf, if_, sizeof(struct intr_frame));

	// if (curr->fork_depth > 179)
	// 	return -1;
	int tid = thread_create(name, PRI_DEFAULT, __do_fork, curr);

	// 	return -1;

	if (tid == TID_ERROR)
	{
		// printf("크리에이트 실패\n");
		return TID_ERROR;
	}
	sema_down(&curr->fork_sema);

	struct child_info *c_info = get_child_info(curr, tid);
	// printf("자식 ret :%d\n", c_info->ret);
	if (c_info == NULL || c_info->ret == -9999)
		// printf("처리해\n");
		return -1;

	return tid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */

/* 부모 프로세스의 주소 공간을 복제하려면 이 함수를 pml4_for_each에 전달하십시오.
 * 이 기능은 프로젝트 2 전용입니다. */
static bool
duplicate_pte(uint64_t *pte, void *va, void *aux)
{
	struct thread *current = thread_current();
	struct thread *parent = (struct thread *)aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */

	/* 2. Resolve VA from the parent's page map level 4. */

	/* 1. TODO: 만약 parent_page가 커널 페이지라면 즉시 반환하세요. */
	if (is_kernel_vaddr(va))
		return true;
	// if (is_kern_pte(pte))
	// 	return false;

	/* 2. 부모의 페이지 맵 레벨 4에서 가상 주소(VA)를 해결하세요. */
	parent_page = pml4_get_page(parent->pml4, va);

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */

	/* 3. TODO: 자식을 위해 새로운 PAL_USER 페이지를 할당하고 결과를 NEWPAGE에 설정하세요. */
	newpage = palloc_get_page(PAL_USER);
	if (newpage == NULL)
	{

		return false;
	}

	/* 4. TODO: 부모의 페이지를 새 페이지로 복제하고
	 *    TODO: 부모의 페이지가 쓰기 가능한지 확인하세요 (결과에 따라 WRITABLE을 설정하세요). */
	memcpy(newpage, parent_page, PGSIZE);

	writable = is_writable(pte);

	/* 5. 주소 VA에 대해 자식의 페이지 테이블에 새 페이지를 WRITABLE 권한으로 추가하세요. */

	if (!pml4_set_page(current->pml4, va, newpage, writable))
	{
		/* 6. TODO: if fail to insert page, do error handling. */
		palloc_free_page(newpage);
		return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
/* 부모 프로세스의 실행 컨텍스트를 복사하는 스레드 함수입니다.
 * 힌트) parent->tf는 프로세스의 사용자 공간 컨텍스트를 유지하지 않습니다.
 *       즉, 이 함수에 process_fork의 두 번째 인자를 전달해야 합니다. */
static void
__do_fork(void *aux)
{

	struct intr_frame if_;
	struct thread *parent = (struct thread *)aux;
	struct thread *current = thread_current();
	// 페이지 폴트 분기
	current->exit_code = -9999;
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	/* TODO: 부모의 인터럽트 플래그를 어떻게든 전달하세요. (예: process_fork()의 if_) */
	struct intr_frame *parent_if = parent->fork_tf;
	bool succ = true;
	// printf("자식 실행중: %s tid: %d\n",current->name,current->tid);

	/* 1. Read the cpu context to local stack. */
	memcpy(&if_, parent_if, sizeof(struct intr_frame));
	palloc_free_page(parent->fork_tf);

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;
	process_activate(current);

#ifdef VM
	current->exe_file = file_duplicate(parent->exe_file);

	parent->spt.spt_pml4 = parent->pml4;
	supplemental_page_table_init(&current->spt);
	if (!supplemental_page_table_copy(&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each(parent->pml4, duplicate_pte, parent))
	{

		goto error;
	}

	// printf("부모스택 복사 완료\n");

#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

	/* TODO: 여기에 코드를 작성하세요.
	 * TODO: 힌트) 파일 객체를 복제하려면 `include/filesys/file.h`에서 `file_duplicate` 함수를 사용하세요.
	 * TODO:       부모는 이 함수가 부모의 자원을 성공적으로 복제할 때까지 fork()에서 반환해서는 안 됩니다.*/

	process_init();
	if (current->fdt == NULL)
	{
		// printf("으악1\n");
		goto error;
	}
	/* Finally, switch to the newly created process. */

	// current->fdt[0].stdio = 0;
	// current->fdt[1].stdio = 0;

	// for (int i = 0; i < MAX_FD; i++)
	// {
	// 	// 표준 입출력 복사
	// 	if (parent->fdt[i].stdio != 0)
	// 	{
	// 		current->fdt[i].stdio = parent->fdt[i].stdio;
	// 		continue;
	// 	}

	// 	// child의 파일이 이미 세팅 되어 있는경우 건너뛴다.
	// 	if (parent->fdt[i].dup2_num != 0 && current->fdt[i].file != NULL)
	// 	{
	// 		struct file *dup_file = file_duplicate(parent->fdt[i].file);
	// 		if (dup_file == NULL)
	// 		{
	// 			// printf("으악1\n");

	// 			goto error;
	// 		}
	// 		for (int j = i; j < MAX_FD; j++)
	// 		{
	// 			// 현재 file i 가 순회중인 j와 같은 경우에만 current에게 파일 세팅
	// 			if (parent->fdt[j].file == parent->fdt[i].file)
	// 			{
	// 				current->fdt[j].file = dup_file;
	// 				current->fdt[j].dup2_num = 1;
	// 			}
	// 		}
	// 	}
	// 	else
	// 	{
	// 		// 표준입출력도 아니고 dup2도 아니고 파일이 열려 있는경우 복사
	// 		if (parent->fdt[i].file != NULL)
	// 		{
	// 			current->fdt[i].file = file_duplicate(parent->fdt[i].file);
	// 			if (current->fdt[i].file == NULL)
	// 			{
	// 				// printf("으악2\n");
	// 				goto error;
	// 			}
	// 		}
	// 	}
	// }

	for (int i = MIN_FD; i < MAX_FD; i++)
	{
		if (parent->fdt[i].file != NULL)
		{
			current->fdt[i].file = file_duplicate(parent->fdt[i].file);
		}
	}

	// for (int i = MIN_FD; i < MAX_FD; i++)
	// {
	// 	if (parent->fdt[i].file != NULL)
	// 	{
	// 		if (parent->fdt[i].dup2_num == 0 && tmp_fdt[i] == NULL)
	// 			tmp_fdt[i] = file_duplicate(parent->fdt[i].file);
	// 		else
	// 		{
	// 			if (tmp_fdt[parent->fdt[i].dup2_num] == NULL)
	// 				tmp_fdt[parent->fdt[i].dup2_num] = file_duplicate(parent->fdt[i].file);
	// 		}
	// 	}
	// }

	// for (int i = MIN_FD; i < MAX_FD; i++)
	// {

	// 	if (parent->fdt[i].dup2_num == 0 && parent->fdt[i].file != NULL)
	// 		current->fdt[i].file = tmp_fdt[i];
	// 	else
	// 	{
	// 		current->fdt[i].file = tmp_fdt[parent->fdt[i].dup2_num];
	// 		current->fdt[i].dup2_num = parent->fdt[i].dup2_num;
	// 	}
	// }

	if_.R.rax = 0;
	// printf("자식 실행중 다시체크: %s tid: %d\n",current->name,current->tid);
	// printf("자식 RIP!!!!!!!!!!!!!!!!!!%p\n",if_.rip);

	if (succ)
	{
		current->exit_code = 0;
		current->child_info->ret = 123456;

		sema_up(&current->parent->fork_sema);
		do_iret(&if_);
	}
error:
	current->child_info->ret = -9999;
	current->exit_code = -9999;
	list_remove(&current->child_info->child_elem);
	struct child_info *trash;
	trash = list_entry(&current->child_info->child_elem, struct child_info, child_elem);
	free(trash);
	sema_up(&current->parent->fork_sema);
	sema_up(&current->parent->wait_sema);
	thread_exit();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int process_exec(void *f_name)
{
	char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */

	/* 스레드 구조체에서 intr_frame을 사용할 수 없습니다.
	 * 이는 현재 스레드가 재스케줄될 때 실행 정보를 구조체 멤버에 저장하기 때문입니다. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup();

	sema_down(&exec_sema);
	/* And then load the binary */
	success = load(file_name, &_if);

	sema_up(&exec_sema);

	/* If load failed, quit. */
	palloc_free_page(file_name);
	if (!success)
		return -1;

	/* Start switched process. */
	do_iret(&_if);
	NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */

/* 스레드 TID가 종료될 때까지 기다리고 그 종료 상태를 반환합니다.
 * 커널에 의해 (즉, 예외로 인해 죽었을 때) 종료된 경우 -1을 반환합니다.
 * TID가 유효하지 않거나 호출 프로세스의 자식이 아니거나 이미 주어진 TID에
 * 대해 process_wait()가 성공적으로 호출된 경우에도 즉시 -1을 반환하고 기다리지 않습니다.
 *
 * 이 함수는 문제 2-2에서 구현될 것입니다. 현재는 아무 작업도 수행하지 않습니다. */

int process_wait(tid_t child_tid UNUSED)
{
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	/* XXX: 힌트) pintos는 process_wait(initd)가 실행되면 종료합니다. 따라서
	 * XXX:       process_wait를 구현하기 전에 여기에 무한 루프를 추가하는 것을 권장합니다. */
	struct thread *curr = thread_current();
	struct child_info *child = NULL;
	tid_t find_child = -1;
	int ret;

	if (list_empty(&curr->child_list))
		return -1;

	while (true)
	{
		find_child = -1;
		for (struct list_elem *cur = list_begin(&curr->child_list); cur != list_end(&curr->child_list); cur = list_next(cur))
		{
			child = list_entry(cur, struct child_info, child_elem);
			if (child->pid == child_tid)
			{
				find_child = child->pid;
				ret = child->ret;
				if (child->status == CHILD_EXIT)
				{
					list_remove(&child->child_elem);
					free(child);
					return ret;
				}
			}
		}
		if (find_child == -1)
		{
			return -1;
		}
		sema_init(&curr->wait_sema, 0);
		sema_down(&curr->wait_sema);
	}
}

void process_exit(void)
{
	struct thread *curr = thread_current();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

	/* TODO: 여기에 코드를 작성하세요.
	 * TODO: 프로세스 종료 메시지를 구현하세요 (project2/process_termination.html 참조).
	 * TODO: 여기에서 프로세스 리소스 정리를 구현하는 것을 권장합니다. */

	if (!curr->is_kernel)
	{

		printf("%s: exit(%d)\n", curr->name, curr->exit_code);

		/* 자식 프로세서 다잉 메시지 정리 */
		if (!list_empty(&curr->child_list))
		{
			struct child_info *trash;
			for (struct list_elem *cur = list_begin(&curr->child_list); cur != list_end(&curr->child_list); cur = list_next(cur))
			{
				trash = list_entry(cur, struct child_info, child_elem);
				// printf("%s다잉 정리\n", curr->name);

				list_remove(cur);
				trash->child_thread->parent = NULL;
				free(trash);
			}
			// printf("다잉 정리끝\n");
		}

		/* 부모 프로세스에 메시지 남김 */
		if (curr->parent != NULL)
		{
			curr->child_info->ret = curr->exit_code;
			curr->child_info->status = CHILD_EXIT;
			if (curr->exe_file != NULL)
				file_close(curr->exe_file);
			sema_up(&curr->parent->wait_sema);
		}
	}

	/* 파일 디스크립터 테이블 정리 */
	if (curr->fdt != NULL)
	{
		for (int i = 0; i < MAX_FD; i++)
		{
			if (curr->fdt[i].file != NULL)
			{
				int open_cnt = file_dec_open_cnt(curr->fdt[i].file);
				if (open_cnt == 0)
					file_close(curr->fdt[i].file);
			}
		}
		palloc_free_multiple(curr->fdt, 3);
	}
	process_cleanup();
}

/* Free the current process's resources. */
static void
process_cleanup(void)
{
	struct thread *curr = thread_current();

#ifdef VM
	supplemental_page_table_kill(&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL)
	{
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */

		/* 이곳에서 올바른 순서 설정이 중요합니다.
		 * 현재 프로세스의 페이지 디렉토리인 cur->pagedir을 NULL로 설정해야 합니다.
		 * 페이지 디렉토리를 전환하기 전에, 타이머 인터럽트가
		 * 프로세스 페이지 디렉토리로 다시 전환되지 않도록 합니다.
		 * 프로세스의 페이지 디렉토리를 파괴하기 전에 기본 페이지
		 * 디렉토리를 활성화해야 합니다. 그렇지 않으면 우리의 활성 페이지 디렉토리는
		 * 이미 해제되고 초기화된 페이지 디렉토리가 될 것입니다. */

		curr->pml4 = NULL;
		pml4_activate(NULL);
		pml4_destroy(pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void process_activate(struct thread *next)
{
	/* Activate thread's page tables. */
	pml4_activate(next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update(next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL 0			/* Ignore. */
#define PT_LOAD 1			/* Loadable segment. */
#define PT_DYNAMIC 2		/* Dynamic linking info. */
#define PT_INTERP 3			/* Name of dynamic loader. */
#define PT_NOTE 4			/* Auxiliary info. */
#define PT_SHLIB 5			/* Reserved. */
#define PT_PHDR 6			/* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr
{
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack(struct intr_frame *if_);
static bool validate_segment(const struct Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes,
						 bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load(const char *file_name, struct intr_frame *if_)
{
	struct thread *t = thread_current();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i, args_cnt = 0, f_len = 0;
	char *token[100], *save_ptr;

	f_len = strlen(file_name) + 1;

	for (token[args_cnt] = strtok_r(file_name, " ", &save_ptr); token[args_cnt] != NULL; token[args_cnt] = strtok_r(NULL, " ", &save_ptr))
	{
		// printf("'%s'\n", token[args_cnt]);
		f_len += strlen(token[args_cnt]) + 1;
		args_cnt++;
	}

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create();
	if (t->pml4 == NULL)
		goto done;

	process_activate(thread_current());

	/* Open executable file. */
	file = filesys_open(token[0]);
	if (file == NULL)
	{
		printf("load: %s: open failed\n", token[0]);
		goto done;
	}

	file_deny_write(file);

	/* Read and verify executable header. */
	if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 0x3E // amd64
		|| ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Phdr) || ehdr.e_phnum > 1024)
	{
		printf("load: %s: error loading executable\n", token[0]);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++)
	{
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length(file))
			goto done;
		file_seek(file, file_ofs);

		if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type)
		{
		case PT_NULL:
		case PT_NOTE:
		case PT_PHDR:
		case PT_STACK:
		default:
			/* Ignore this segment. */
			break;
		case PT_DYNAMIC:
		case PT_INTERP:
		case PT_SHLIB:
			goto done;
		case PT_LOAD:
			if (validate_segment(&phdr, file))
			{
				bool writable = (phdr.p_flags & PF_W) != 0;
				uint64_t file_page = phdr.p_offset & ~PGMASK;
				uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
				uint64_t page_offset = phdr.p_vaddr & PGMASK;
				uint32_t read_bytes, zero_bytes;
				if (phdr.p_filesz > 0)
				{
					/* Normal segment.
					 * Read initial part from disk and zero the rest. */
					read_bytes = page_offset + phdr.p_filesz;
					zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
				}
				else
				{
					/* Entirely zero.
					 * Don't read anything from disk. */
					read_bytes = 0;
					zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
				}
				if (!load_segment(file, file_page, (void *)mem_page,
								  read_bytes, zero_bytes, writable))
					goto done;
			}
			else
				goto done;
			break;
		}
	}

	/* Set up stack. */
	if (!setup_stack(if_))
		goto done;
	// if_->R.rbp = if_->rsp;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	int args_size = ROUND_UP(f_len, 8);
	if_->rsp -= args_size;
	uintptr_t tmp_rsp = if_->rsp;
	for (int i = 0; i < args_cnt; i++)
	{
		int t_len = strlen(token[i]) + 1;
		memcpy(tmp_rsp, token[i], t_len);
		// printf("=============== %s\n",if_->rsp);
		tmp_rsp += t_len;
	}
	uintptr_t args_cur = if_->rsp;
	// printf("출력: %s\n",args_cur);

	if_->rsp -= 8;
	memset(if_->rsp, (uint8_t)0, sizeof(uintptr_t));
	if_->rsp -= (args_cnt * 8);
	tmp_rsp = if_->rsp;
	if_->R.rsi = if_->rsp;

	for (int i = 0; i < args_cnt; i++)
	{
		// printf("+++++++++++++++++ %s\n",args_cur);

		__asm __volatile(
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%rcx, (%%rax)\n"
			: : "g"(tmp_rsp), "g"(args_cur) : "memory");
		// memcpy(tmp_rsp,&args_cur,8);
		// printf("==============: %s\n",args_cur);
		tmp_rsp += 8;
		args_cur += strlen(token[i]) + 1;
	}

	if_->rsp -= 8;
	memset(if_->rsp, (uint8_t)0, sizeof(uintptr_t));

	if_->R.rdi = (uint64_t)args_cnt;
	success = true;
done:
	/* We arrive here whether the load is successful or not. */
	/* 실행파일을 write 하지 못하도록 */
	t->exe_file = file;
	// file_close(file);
	return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Phdr *phdr, struct file *file)
{
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t)file_length(file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr((void *)phdr->p_vaddr))
		return false;
	if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page(void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
			 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page(PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
		{
			palloc_free_page(kpage);
			return false;
		}
		memset(kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page(upage, kpage, writable))
		{
			printf("fail\n");
			palloc_free_page(kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack(struct intr_frame *if_)
{
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	if (kpage != NULL)
	{
		success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page(kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct thread *curr = thread_current();
	struct lazy_file *lf = aux;
	struct file *file = curr->exe_file;
	off_t ofs = lf->ofs;
	bool writable = lf->writable;

	file_seek(file, ofs);

	/* Do calculate how to fill this page.
	 * We will read PAGE_READ_BYTES bytes from FILE
	 * and zero the final PAGE_ZERO_BYTES bytes. */
	size_t page_read_bytes = lf->page_read_bytes;
	size_t page_zero_bytes = lf->page_zero_bytes;

	uint8_t *kpage = page->frame->kva;
	if (kpage == NULL)
		return false;

	/* Load this page. */
	if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
	{
		// palloc_free_page(kpage);
		return false;
	}
	memset(kpage + page_read_bytes, 0, page_zero_bytes);

	// printf("페이지\n");
	// printf("끝 레이지\n\n");

	free(aux);
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */

/* 파일에서 주어진 오프셋(OFS)에서 시작하는 세그먼트를 주어진 주소(UPAGE)에 로드합니다.
 * 총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 초기화됩니다. 초기화 방법은 다음과 같습니다:
 *
 * - UPAGE에서 시작하는 READ_BYTES 바이트는 파일의 오프셋 OFS에서 읽어와야 합니다.
 *
 * - UPAGE + READ_BYTES에서 시작하는 ZERO_BYTES 바이트는 0으로 설정되어야 합니다.
 *
 * 이 함수에 의해 초기화된 페이지는 WRITABLE이 true이면 사용자 프로세스에 의해 쓰기 가능해야 하며,
 * 그렇지 않으면 읽기 전용이어야 합니다.
 *
 * 성공하면 true를 반환하고, 메모리 할당 오류나 디스크 읽기 오류가 발생하면 false를 반환합니다. */

static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
			 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	off_t ofs_curr = ofs;
	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// void *aux = NULL;
		struct lazy_file *aux = malloc(sizeof(struct lazy_file));
		aux->ofs = ofs_curr;
		ofs_curr += page_read_bytes;
		// printf("읽을 위치 %d\n",file_tell(file));

		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		aux->writable = writable;

		if (!vm_alloc_page_with_initializer(VM_ANON, upage,
											writable, lazy_load_segment, aux))
			return false;

		// uint8_t *kpage = palloc_get_page(PAL_USER);
		// if (kpage == NULL)
		// 	return false;

		// /* Load this page. */
		// if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
		// {
		// 	palloc_free_page(kpage);
		// 	return false;
		// }
		// memset(kpage + page_read_bytes, 0, page_zero_bytes);

		// struct thread *t = thread_current();

		// uint8_t *rd_upage = pg_round_down(upage);
		// // printf("페이지\n");

		// if (spt_find_page(&t->spt, rd_upage))
		// {
		// 	pml4_set_page(t->pml4, rd_upage, kpage, writable);
		// }
		// else
		// {
		// 	return false;
		// }

		/* Advance. */
		// printf("aa\n");
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack(struct intr_frame *if_)
{
	bool success = false;
	void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */
	// struct thread *curr = thread_current();
	// struct supplemental_page_table *spt = &curr->spt;

	// uint8_t *newpage;
	// newpage = palloc_get_page(PAL_USER | PAL_ZERO);
	// if (newpage == NULL)
	// 	return false;

	uint8_t *rd_upage = pg_round_down(stack_bottom);
	// vm_alloc_page(VM_ANON, rd_upage, true);
	// printf("시작 스택 %p\n",rd_upage);
	success = vm_claim_page(rd_upage);
	// 	struct page *new_page = malloc(sizeof(struct page));
	// 	struct frame *new_frame = malloc(sizeof(struct frame));
	// 	new_page->writable = true;
	// 	// uninit_new(new_page, rd_upage, NULL, VM_ANON, NULL, anon_initializer);
	// 	spt_insert_page(spt, new_page);
	// 	new_frame->kva = newpage;
	// 	new_frame->page = new_page;
	// 	new_page->frame = new_frame;
	// 	new_page->va = rd_upage;
	// 	if (pml4_set_page(curr->pml4, rd_upage, newpage, true) == NULL)
	// 	{
	// 		palloc_free_page(newpage);
	// 		return false;
	// 	}

	// 	// printf("%d에 저장 %p, %p \n\n", spt->pg_cnt, stack_bottom, spt->pg[spt->pg_cnt].va);
	// }

	if_->rsp = (uint8_t *)USER_STACK;
	return true;
}
#endif /* VM */

struct child_info *get_child_info(struct thread *curr, int child_pid)
{
	struct child_info *child = NULL;
	int find_child = -1;
	for (struct list_elem *cur = list_begin(&curr->child_list); cur != list_end(&curr->child_list); cur = list_next(cur))
	{
		child = list_entry(cur, struct child_info, child_elem);
		if (child->pid == child_pid)
			return child;
	}

	return NULL;
}