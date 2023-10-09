#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"

struct inode;


struct file_fd
{
    int stdio;
    int dup2_num;
    void * file;
};


/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
struct file *file_duplicate (struct file *file);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* inode 락 반환 함수 */
struct lock* file_get_inode_lock(struct file *file);
int file_inc_open_cnt(struct file *file);
int file_dec_open_cnt(struct file *file);

/* Preventing writes. */
void file_deny_write (struct file *);
int file_is_deny(struct file *file);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

#endif /* filesys/file.h */
