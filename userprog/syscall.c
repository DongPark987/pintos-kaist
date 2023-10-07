#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/exception.h"
#include "devices/input.h"
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
    f->R.rax = sys_fork(f);
    break;
  case SYS_EXEC:
    f->R.rax = exec(f->R.rdi);
    break;
  case SYS_WAIT:
    f->R.rax = wait(f->R.rdi);
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
    f->R.rax = filesize(f->R.rdi);
    break;
  case SYS_READ:
    f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
    break;
  case SYS_WRITE:
    f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
    break;
  case SYS_SEEK:
    seek(f->R.rdi, f->R.rsi);
    break;
  case SYS_TELL:
    f->R.rax = tell(f->R.rdi);
    break;
  case SYS_CLOSE:
    close(f->R.rdi);
    break;
  default:
    break;
  }
}

/*
Terminates Pintos by calling power_off() (declared in src/include/threads/init.h).
This should be seldom used, because you lose some information about possible deadlock situations, etc.
*/
void halt(void)
{
  // Terminating a process implicitly closes all its open file descriptors
  for (int fd = 0; fd < thread_current()->fd_cnt; fd++)
    close(fd);
  power_off();
}

/*
Terminates the current user program, returning status to the kernel.
If the process's parent waits for it (see below), this is the status that will be returned.
Conventionally, a status of 0 indicates success and nonzero values indicate errors.
*/
void exit(int status)
{
  // TODO: Exiting a process implicitly closes all its open file descriptors
  // for (int fd = 0; fd < MAX_FD; fd++)
  // {
  //     close(fd);
  // }

  thread_current()->tf.R.rdi = status;
  thread_exit();
}

/*
Creates a new file called file initially initial_size bytes in size.
Returns true if successful, false otherwise.
Creating a new file does not open it:
opening the new file is a separate operation which would require a open system call.
*/
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
  thread_current()->fd_cnt++;
  thread_current()->fdt[thread_current()->fd_cnt] = opened_file;

  return thread_current()->fd_cnt;
}

/*
Closes file descriptor fd.
Exiting or terminating a process implicitly closes all its open file descriptors,
as if by calling this function for each one.
*/
void close(int fd)
{
  if (thread_current()->fdt[fd] != NULL)
  {
    file_close(thread_current()->fdt[fd]);
    thread_current()->fdt[fd] = NULL;
    thread_current()->fd_cnt--;
  }
}

/*
Reads size bytes from the file open as fd into buffer.
Returns the number of bytes actually read (0 at end of file),
or -1 if the file could not be read (due to a condition other than end of file).
fd 0 reads from the keyboard using input_getc().
*/
int read(int fd, void *buffer, unsigned size)
{
  if (fd < 0 || fd > MAX_FD)
    exit(-1);

  if (fd == 0)
    return input_getc();

  return file_read(thread_current()->fdt[fd], buffer, size);
}

/*
Returns the size, in bytes, of the file open as fd.
*/
int filesize(int fd)
{
  return file_length(thread_current()->fdt[fd]);
}

/*
Changes the next byte to be read or written in open file fd to position, expressed in bytes from the beginning of the file
(Thus, a position of 0 is the file's start).
A seek past the current end of a file is not an error.
A later read obtains 0 bytes, indicating end of file.
A later write extends the file, filling any unwritten gap with zeros.
(However, in Pintos files have a fixed length until project 4 is complete, so writes past end of file will return an error.)
These semantics are implemented in the file system and do not require any special effort in system call implementation.
*/
void seek(int fd, unsigned position)
{
  file_seek(thread_current()->fdt[fd], position);
}

/*
Returns the position of the next byte to be read or written in open file fd, expressed in bytes from the beginning of the file.
*/
unsigned tell(int fd)
{
  return thread_current()->fdt[fd]->pos;
}

/*
Writes size bytes from buffer to the open file fd.
Returns the number of bytes actually written, which may be less than size if some bytes could not be written.
*/
int write(int fd, const void *buffer, unsigned size)
{
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }

  return file_write(thread_current()->fdt[fd], buffer, size);
}

/*
Create new process which is the clone of current process with the name THREAD_NAME.
You don't need to clone the value of the registers except %RBX, %RSP, %RBP, and %R12 - %R15, which are callee-saved registers.
Must return pid of the child process, otherwise shouldn't be a valid pid.
In child process, the return value should be 0.
The child should have DUPLICATED resources including file descriptor and virtual memory space.
Parent process should never return from the fork until it knows whether the child process successfully cloned.
That is, if the child process fail to duplicate the resource, the fork () call of parent should return the TID_ERROR.

The template utilizes the pml4_for_each() in threads/mmu.c to copy entire user memory space, including corresponding pagetable structures,
but you need to fill missing parts of passed pte_for_each_func

*/
// pid_t fork(const char *thread_name)
pid_t sys_fork(struct intr_frame *f)
{
  return process_fork(f->R.rdi, f);
}

/*

Waits for a child process pid and retrieves the child's exit status.
If pid is still alive, waits until it terminates.
Then, returns the status that pid passed to exit.
If pid did not call exit(), but was terminated by the kernel (e.g. killed due to an exception), wait(pid) must return -1.
It is perfectly legal for a parent process to wait for child processes that have already terminated by the time the parent calls wait,
but the kernel must still allow the parent to retrieve its child’s exit status, or learn that the child was terminated by the kernel.

wait must fail and return -1 immediately if any of the following conditions is true:
*/
int wait(pid_t pid)
{
  return process_wait(pid);
}

int exec(const char *cmd_line)
{
  return process_exec(cmd_line);
}