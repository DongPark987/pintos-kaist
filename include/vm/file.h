#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;
struct lazy_mmap{
  struct file *file;

  bool writable;
  off_t offset;
  uint32_t page_read_bytes;
  size_t length;
  void *addr;
  void *upage;
};

struct file_page {
  size_t page_length;
  size_t total_length;
  void *addr;
  void *upage;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
bool *do_lazy_mmap(struct page *page, void *aux);
#endif
