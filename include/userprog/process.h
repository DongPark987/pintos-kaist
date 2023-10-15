#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Max file descriptor */
#define MAX_FD 500
#define MIN_FD 4
#define STDIN_FILENO 1
#define STDOUT_FILENO 2



tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

struct child_info *get_child_info(struct thread *curr, int child_pid);

#endif /* userprog/process.h */
