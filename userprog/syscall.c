#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "filesys/file.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/gdt.h"
#include "userprog/process.h"

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

void syscall_init(void) {
  write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG)
                                                               << 32);
  write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

  /* The interrupt service rountine should not serve any interrupts
   * until the syscall_entry swaps the userland stack to the kernel
   * mode stack. Therefore, we masked the FLAG_FL. */
  write_msr(MSR_SYSCALL_MASK,
            FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* System Call Implementation */

/* power off system */
void halt(void) {
  power_off();
  NOT_REACHED();
}

/* terminate current process */
void exit(int status) {
  thread_current()->tf.R.rdi = status;
  thread_exit();
  return;
}

/* create a new file */
bool create(const char *file, unsigned initial_size) {
  if (file == NULL) exit(-1);  // 잘못된 포인터인 경우 즉시 종료
  return filesys_create(file, initial_size);
}

/* return free fd */
// TODO: linked-list로
static int free_fd() {
  if (thread_current()->fd_cnt >= MAX_FD) return -1;

  for (int fd = MIN_FD; fd < MAX_FD; fd++) {
    if (thread_current()->fdt[fd] == NULL) {
      ASSERT(fd >= MIN_FD);
      return fd;
    }
  }
}

/* open a new file */
int open(const char *file_name) {
  if (file_name == NULL) return -1;

  int fd = free_fd();
  struct file *file = filesys_open(file_name);

  if (fd < MIN_FD || file == NULL) return -1;

  thread_current()->fdt[fd] = file;
  thread_current()->fd_cnt++;

  return fd;
}

/* close a file */
void close(int fd) {
  struct file *file = thread_current()->fdt[fd];
  if (fd < MIN_FD || file == NULL) return;

  file_close(file);
  thread_current()->fdt[fd] = NULL;
  thread_current()->fd_cnt--;
}

/* read from file (to buffer) */
int read(int fd, void *buffer, unsigned size) {
  if (fd == STDIN_FILENO) {
    return input_getc();
  }
  struct file *file = thread_current()->fdt[fd];
  if (file == NULL) return -1;
  return file_read(file, buffer, size);
}

/* write buffer to file */
int write(int fd, const void *buffer, unsigned size) {
  if (fd == STDOUT_FILENO) {
    putbuf((char *)buffer, size);
    return size;
  }
  struct file *file = thread_current()->fdt[fd];
  if (file == NULL) return -1;
  return file_write(file, buffer, size);
}

int filesize(int fd) {
  if (fd < MIN_FD) return -1;
  struct file *file = thread_current()->fdt[fd];
  if (file == NULL) return -1;
  return file_length(file);
}

tid_t fork(const char *thread_name, struct intr_frame *user_if) {
  //
  return process_fork(thread_name, user_if);
}

int wait(tid_t tid) {
  //
  return process_wait(tid);
}

int exec(const char *cmd_line) {
  const char *fn_copy = palloc_get_page(0);
  strlcpy(fn_copy, cmd_line, strlen(cmd_line) + 1);
  int success = process_exec(fn_copy);
  if (success < 0) exit(-1);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f) {
  switch (f->R.rax) {
    case SYS_HALT: {
      halt();
      break;
    }
    case SYS_EXIT: {
      exit((int)f->R.rdi);
      break;
    }
    case SYS_FORK: {
      f->R.rax = fork((const char *)f->R.rdi, f);
      break;
    }
    case SYS_EXEC: {
      f->R.rax = exec((const char *)f->R.rdi);
      break;
    }
    case SYS_WAIT: {
      f->R.rax = wait((tid_t)f->R.rdi);
      break;
    }
    case SYS_CREATE: {
      f->R.rax = create((char *)f->R.rdi, f->R.rsi);
      break;
    }
    case SYS_REMOVE:
      break;
    case SYS_OPEN: {
      f->R.rax = open((const char *)f->R.rdi);
      break;
    }
    case SYS_FILESIZE: {
      f->R.rax = filesize((int)f->R.rdi);
      break;
    }
    case SYS_READ: {
      f->R.rax = read((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
      break;
    }
    case SYS_WRITE: {
      f->R.rax = write((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
      break;
    }
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE: {
      close((int)f->R.rdi);
      break;
    }
    default:
      break;
  }
}
