#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <debug.h>
#include <stddef.h>

void syscall_init(void);

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);

#endif /* userprog/syscall.h */
