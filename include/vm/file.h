#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct lazy_mmap
{
	struct page *head_page;
	void *addr;
	void *upage;
	size_t length;
	size_t page_read_bytes;
	int writable;
	struct file *file;
	off_t offset;
};

struct file_page
{
	/* 파일백드 헤드 */
	struct page *head_page;
	
	/* 연결된 파일 */
	struct file *file;
	
	/* 파일백드의 헤드 */
	void *addr;
	
	/* 현재 파일백드 페이지가 연결된 가상 주소 */
	void *upage;
	
	/* 현재 파일 백드 페이지의 길이 */
	size_t page_length;
	
	/* 전체 파일 백드 페이지의 길이 */
	size_t total_length;
	
	/* 현재 파일백드 페이지에 해당하는 파일의 오프셋 */
	off_t offset;
};

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
			  struct file *file, off_t offset);
void do_munmap(void *va);
bool *do_lazy_mmap(struct page *page, void *aux);
#endif
