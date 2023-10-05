#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

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
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void exit(int status)
{
	// printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_current()->tf.R.rdi = status;
	thread_exit();
}

bool file_create(char *str, off_t size)
{
	// bool result = pml4e_walk(base_pml4,str,0);
	if (str == NULL)
		exit(-1);
	return filesys_create(str, size);
}

static int find_free_td(struct thread *cur_t)
{
	for (int i = 3; i < 128; i++)
	{
		if (cur_t->fdt[i] == 0)
			return i;
	}
	return -1;
}

int fd_open(char *str)
{
	struct thread *cur_t = thread_current();
	struct file *f;
	if (str == NULL)
		return -1;
	int fd = find_free_td(cur_t);
	if (fd == -1)
		return -1;
	f = filesys_open(str);
	if (f == NULL)
		return -1;
	cur_t->fdt[fd] = f;
	return fd;
}

int fd_close(int fd)
{
	struct thread *cur_t = thread_current();
	if (cur_t->fdt[fd] == 0)
		return -1;
	file_close(cur_t->fdt[fd]);
	cur_t->fdt[fd] = 0;
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
		break;
	case SYS_EXEC:
		break;
	case SYS_WAIT:
		break;
	case SYS_CREATE:
		f->R.rax = file_create(f->R.rdi, f->R.rsi);
		// printf("hello\n");
		break;
	case SYS_REMOVE:
		break;
	case SYS_OPEN:
		f->R.rax = fd_open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		break;
	case SYS_READ:
		break;
	case SYS_WRITE:
		putbuf(f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		break;
	case SYS_TELL:
		break;
	case SYS_CLOSE:
		fd_close(f->R.rdi);
		break;

	default:
		break;
	}

	// thread_exit ();
}
