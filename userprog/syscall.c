#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "userprog/process.h"
#include "threads/palloc.h"
#include <string.h>

#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

/* 시스템 호출.
 *
 * 이전에는 시스템 호출 서비스가 인터럽트 핸들러로 처리되었습니다
 * (예: 리눅스에서의 int 0x80). 그러나 x86-64에서는 제조업체가
 * 시스템 호출을 요청하기 위한 효율적인 경로를 제공합니다. `syscall` 명령어입니다.
 *
 * Syscall 명령어는 Model Specific Register (MSR)에서 값들을 읽어와 동작합니다.
 * 자세한 내용은 매뉴얼을 참조하십시오. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */

	/* 시스템 호출 진입점이 사용자 영역 스택을 커널 모드 스택으로 교체할 때까지
	 * 인터럽트 서비스 루틴은 어떠한 인터럽트도 처리해서는 안 됩니다.
	 * 따라서 우리는 FLAG_FL을 마스킹하여 이를 방지합니다. */

	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void exit(int status)
{
	thread_current()->exit_code = status;
	thread_exit();
}

bool file_create(char *str, off_t size)
{
	// bool result = pml4e_walk(base_pml4,str,0);
	if (str == NULL)
		exit(-1);
	// if(is_kernel_vaddr(str))
	// 	exit(-1);
	return filesys_create(str, size);
}

static int find_free_fd(struct thread *cur_t)
{
	for (int i = MIN_FD; i < MAX_FD; i++)
	{
		if (cur_t->fdt[i].file == 0)
			return i;
	}
	return -1;
}

int is_std_io_open(int fd)
{
	struct thread *curr = thread_current();
	if (curr->fdt[fd].file == 1)
		return fd;
	else
		return 0;
}

int fd_open(char *str)
{
	struct thread *curr = thread_current();
	struct file *f;
	if (str == NULL)
		return -1;
	int fd = find_free_fd(curr);
	if (fd == -1)
		return -1;
	f = filesys_open(str);
	if (f == NULL)
		return -1;
	curr->fdt[fd].file = f;
	curr->fd_cnt++;
	return fd;
}

void fd_close(int fd)
{
	struct thread *curr = thread_current();

	/* 유효하지 않은 fd */
	if (fd < 0)
		return;

	/* 표준 입출력 fd 닫기 */
	if (curr->fdt[fd].stdio != 0)
	{
		curr->fdt[fd].stdio = 0;
		return;
	}

	/* 사용되지 않은 fd */
	if (curr->fdt[fd].file == 0)
		return;

	int open_cnt = file_dec_open_cnt(curr->fdt[fd].file);
	if (open_cnt == 0)
		file_close(curr->fdt[fd].file);

	curr->fdt[fd].file = 0;
	curr->fdt[fd].dup2_num = 0;
	curr->fdt[fd].stdio = 0;
	curr->fd_cnt--;
}

int fd_read(int fd, void *buffer, size_t size)
{
	struct thread *curr = thread_current();
	/* 유효하지 않은 fd */
	if (fd < 0)
		return -1;

	/* 표준 입출력 fd 읽기 */
	if (curr->fdt[fd].stdio != 0)
	{
		if (curr->fdt[fd].stdio == STDIN_FILENO)
			return input_getc();
		return -1;
	}

	off_t read_size = 0;

	if (curr->fdt[fd].file == NULL)
		return -1;
	lock_acquire(file_get_inode_lock(curr->fdt[fd].file));
	read_size = file_read(curr->fdt[fd].file, buffer, size);
	lock_release(file_get_inode_lock(curr->fdt[fd].file));
	return read_size;
}

int fd_file_size(int fd)
{
	if (fd < 0)
		return 0;
	struct thread *cur_t = thread_current();
	return file_length(cur_t->fdt[fd].file);
}

int fd_write(int fd, char *buffer, unsigned size)
{
	struct thread *curr = thread_current();
	off_t write_size = 0;

	/* 유효하지 않은 fd */
	if (fd < 0)
		return 0;

	if (curr->fdt[fd].stdio != 0)
	{
		if (curr->fdt[fd].stdio == STDOUT_FILENO)
		{
			putbuf(buffer, size);
			return size;
		}
		return 0;
	}

	if (file_is_deny(curr->fdt[fd].file) || curr->fdt[fd].file == NULL)
		return 0;
	else
	{
		lock_acquire(file_get_inode_lock(curr->fdt[fd].file));
		write_size = file_write(curr->fdt[fd].file, buffer, size);
		lock_release(file_get_inode_lock(curr->fdt[fd].file));
		return write_size;
	}
}

int sys_fork(struct intr_frame *f UNUSED)
{
	struct thread *curr = thread_current();
	// printf("부모 RIP!!!!!!!!!!!!!!!!!!%p\n",f->rip);
	int tid = process_fork(f->R.rdi, f);
	// printf("난 부모%d, 만든자식: %d\n", curr->tid, tid);
	return tid;
}

int exec(const char *cmd_line)
{
	char *file_name = palloc_get_page(0);
	strlcpy(file_name, cmd_line, strlen(cmd_line) + 1);
	if (process_exec(file_name) == -1)
		exit(-1);
}

void fd_seek(int fd, unsigned position)
{
	struct thread *curr = thread_current();

	if (fd < 0)
		return;

	if (curr->fdt[fd].stdio != 0)
		return;

	if (curr->fdt[fd].file == NULL)
		return;
	file_seek(curr->fdt[fd].file, position);
}

int dup2(int oldfd, int newfd)
{
	struct thread *curr = thread_current();
	// printf("%d를 %d로 덥트한다.\n",newfd,oldfd);
	// 둘중 하나라도 유효하지 않은 fd면 실패
	if (oldfd < 0 || newfd < 0)
		return -1;

	/* 표준 입출력 dup */
	if (curr->fdt[oldfd].stdio != 0)
	{
		// 표준 입출력이며 fd가 같으면 그냥 반환
		if (newfd == oldfd)
			return newfd;

		// new가 표준 입출력이거나 파일이 열려 있는 상태라면 close
		if (curr->fdt[newfd].stdio != 0 || curr->fdt[newfd].file != NULL)
			fd_close(newfd);

		// 표준 입출력 fd로 변환
		curr->fdt[newfd].stdio = curr->fdt[oldfd].stdio;
		curr->fdt[newfd].dup2_num = 0;

		return newfd;
	}

	//oldfd가 닫혀있으면 실패
	if (curr->fdt[oldfd].file == 0)
		return -1;

	//유효한데 같은 fd면 그냥 반환
	if (newfd == oldfd)
		return newfd;

	//열려 있으면 닫는다.
	if (curr->fdt[newfd].file != 0)
		fd_close(newfd);
		
	

	curr->fdt[newfd].file = curr->fdt[oldfd].file;
	curr->fdt[newfd].dup2_num = 1;
	curr->fdt[newfd].stdio = 0;
	file_inc_open_cnt(curr->fdt[newfd].file);
	return newfd;
}

unsigned fd_tell (int fd){
	struct thread *curr = thread_current();
	if(curr->fdt[fd].file==0)
		return 0;
	return file_tell(curr->fdt[fd].file);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	/* The main system call interface */
	switch (f->R.rax)
	{
	case SYS_HALT:
		power_off();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
	case SYS_FORK:
		f->R.rax = sys_fork(f);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = process_wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = file_create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		break;
	case SYS_OPEN:
		f->R.rax = fd_open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = fd_file_size(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = fd_read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = fd_write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		fd_seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = fd_tell (f->R.rdi);
		break;
	case SYS_CLOSE:
		fd_close(f->R.rdi);
		break;

	case SYS_DUP2:
		f->R.rax = dup2(f->R.rdi, f->R.rsi);
		break;

	default:
		break;
	}
}
