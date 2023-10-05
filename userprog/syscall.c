#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/exception.h"
#include <string.h>

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

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
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

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{

  /* The main system call interface */
  switch (f->R.rax)
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit(f->R.rdi);
    break;
  case SYS_FORK:
    break;
  case SYS_EXEC:
    break;
  case SYS_WAIT:
    break;
  case SYS_CREATE:
    f->R.rax = create(f->R.rdi, f->R.rsi);
    break;
  case SYS_REMOVE:
    break;
  case SYS_OPEN:
    f->R.rax = open(f->R.rdi);
    break;
  case SYS_FILESIZE:
    break;
  case SYS_READ:
    break;
  case SYS_WRITE:
    printf("%s", f->R.rsi);
    break;
  case SYS_SEEK:
    break;
  case SYS_TELL:
    break;
  case SYS_CLOSE:
    break;

  default:
    break;
  }
}

void halt(void)
{
  power_off();
}

void exit(int status)
{
  thread_current()->tf.R.rdi = status;
  thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
  if (file != NULL && strlen(file) == 0)
    return false;
  else if (file == NULL)
    exit(-1);
  else
    return filesys_create(file, initial_size);
}

/*
Opens the file called file.
Returns a nonnegative integer handle called a "file descriptor" (fd), or -1 if the file could not be opened.
File descriptors numbered 0 and 1 are reserved for the console
: fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output.
The open system call will never return either of these file descriptors,
which are valid as system call arguments only as explicitly described below.

Each process has an independent set of file descriptors.
File descriptors are inherited by child processes.
When a single file is opened more than once, whether by a single process or different processes, each open returns a new file descriptor.
Different file descriptors for a single file are closed independently in separate calls to close and they do not share a file position.
You should follow the linux scheme, which returns integer starting from zero, to do the extra.
*/
int open(const char *file)
{
  if (file == NULL)
    exit(-1);
  struct file *opened_file = filesys_open(file);
  if (opened_file == NULL)
    return -1;
  *thread_current()->fdt = filesys_open(file);
  thread_current()->fd_cnt++;
  return thread_current()->fd_cnt;
}