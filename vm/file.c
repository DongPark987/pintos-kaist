/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void) {}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva) {
  /* Set up the handler */
  page->operations = &file_ops;

  struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in(struct page *page, void *kva) {
  struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out(struct page *page) {
  struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* 파일로 지원되는 페이지를 파괴합니다. 페이지 자체는 호출자에 의해 해제될
 * 것입니다. */
static void file_backed_destroy(struct page *page) {
  // 페이지의 파일 페이지 구조체를 가져옵니다.
  struct file_page *file_page UNUSED = &page->file;
  // 페이지와 관련된 프레임이 존재하는 경우, 해당 프레임의 페이지 포인터를
  // NULL로 설정합니다.
  if (page->frame)
    page->frame->page = NULL;
  free(page->frame);
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset) {
  if (addr == NULL || length == NULL)
    return false;

  struct thread *curr = thread_current();
  struct supplemental_page_table *spt = &curr->spt;
  uint8_t *rd_page = pg_round_down(addr);

  off_t read_start = offset;
  uint32_t read_bytes = length; //  그냥 length일지도

  while (read_bytes > 0) {

    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;

    struct lazy_mmap *aux =
        (struct lazy_mmap *)calloc(1, sizeof(struct lazy_mmap));
    if (aux == NULL) {
      return false;
    }

    aux->page_read_bytes = page_read_bytes;
    aux->writable = writable;
    aux->file = file;
    aux->offset = read_start;
    aux->upage = rd_page;
    aux->addr = addr;
    aux->length = length;

    // 가상 메모리에 페이지를 할당, 페이지 초기화
    // 함수(lazy_load_segment)전달
    if (!vm_alloc_page_with_initializer(VM_FILE, rd_page, writable,
                                        do_lazy_mmap, aux))
      return NULL;

    /* Advance. */
    read_bytes -= page_read_bytes;
    read_start += page_read_bytes;
    rd_page += PGSIZE;
  }
  //   return true;
  // printf("addr = %p", addr);
  return addr;
}

/* Do the munmap */
void do_munmap(void *addr) {}
/* do_lazy_mmap 함수는 파일 데이터를 가상 메모리 페이지로 로드하는 역할을
 * 합니다. 이 함수는 페이지 테이블의 page에 대한 정보와 aux 구조체를 사용하여
 * 파일 데이터를 읽어와 가상 메모리 페이지로 매핑합니다. */
bool *do_lazy_mmap(struct page *page, void *aux) {

  if (page == NULL)
    return false;

  struct thread *curr = thread_current();
  struct lazy_mmap *lm = (struct aux *)aux;
  struct file *file = lm->file;
  bool writable = lm->writable;
  off_t ofs = lm->offset;

  file_seek(file, ofs);

  uint32_t page_read_bytes = lm->page_read_bytes;
  uint8_t *kpage = page->frame->kva;
  if (kpage == NULL)
    return false;

  page->file.addr = lm->addr;
  page->file.upage = lm->upage;
  page->file.page_length = lm->page_read_bytes;
  page->file.total_length = lm->length;

  // uint8_t rd_page = pg_round_down(kpage);

  /* do_lazy_mmap 함수에서는 파일 데이터를 읽어오기만 하고,
     이 데이터의 일부가 누락되더라도 다른 부분은 나중에 읽거나 요청될 수
     있습니다.
     따라서 file_read 함수의 반환 값을 확인하거나 오류 처리를 하지
     않는 것이 일반적입니다.*/

  // 파일 데이터를 읽어와 가상 메모리 페이지에 복사
  file_read(file, kpage, page_read_bytes);

  // 사용한 aux 메모리를 해제
  free(aux);
  return true;
}