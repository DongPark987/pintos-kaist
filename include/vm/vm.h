#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "threads/mmu.h"
#include "list.h"
#include "hash.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* 프로세스 실행 파일을 lazy 하게 로드하기 위한 구조체 */
struct lazy_file
{
    off_t ofs;
    uint32_t page_read_bytes;
    uint32_t page_zero_bytes;
    bool writable;

};


/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */

 /* "페이지"의 표현.
 * 이는 네 개의 "하위 클래스"인 uninit_page, file_page, anon_page 및 페이지 캐시 (프로젝트4)를 가지고 있는
 * 종류의 "상위 클래스"입니다.
 * 이 구조체의 미리 정의된 멤버를 제거하거나 수정하지 마십시오. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
	/* 해시테이블에 기록하기 위한 elem */
	struct hash_elem hash_elem;
	bool writable;

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	int link_cnt;
	struct list_elem elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */

/* 페이지 작업에 대한 함수 테이블.
 * 이것은 C에서 "인터페이스"를 구현하는 한 가지 방법입니다.
 * "메소드"의 테이블을 구조체 멤버로 넣고
 * 필요할 때마다 호출하세요. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* 현재 프로세스의 메모리 공간 표현.
 * 이 구조체에 대해 특정 디자인을 강제하고 싶지 않습니다.
 * 이에 대한 모든 디자인은 여러분에게 달려있습니다. */
struct supplemental_page_table {
	struct hash hash_pt;
	uint64_t *spt_pml4
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

/* kva와 uva를 pml4로 연결하는 함수 */
bool va_set_page(void *va, void *kva, bool writable);
/* malloc 예외처리용 함수 */
void va_exeception_free(void *f);
/* 페이지를 찾는 함수 */
struct page *page_lookup(const void *address, struct supplemental_page_table *spt);
#endif  /* VM_VM_H */
