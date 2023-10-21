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
  struct lazy_mmap *lm = page->uninit.aux;
  page->file.addr = lm->addr;
  page->file.file = lm->file;
  page->file.page_length = lm->page_read_bytes;
  page->file.total_length = lm->length;
  page->file.upage = lm->upage;
  page->file.offset = lm->offset;
  page->file.head_page = lm->head_page;
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
// static void file_backed_destroy(struct page *page) {
//   // 페이지의 파일 페이지 구조체를 가져옵니다.
//   // struct file_page *file_page UNUSED = &page->file;
//   // // 페이지와 관련된 프레임이 존재하는 경우, 해당 프레임의 페이지 포인터를
//   // // NULL로 설정합니다.
//   // if (page->frame)
//   //   page->frame->page = NULL;
//   // free(page->frame);
//   struct file_page *file_page UNUSED = &page->file;
//   if (pml4_is_dirty(thread_current()->pml4, page->va)) {
//     file_write_at(file_page->file, page->va, file_page->total_length,
//                   file_page->offset);
//     pml4_set_dirty(thread_current()->pml4, page->va, 0);
//   }
//   pml4_clear_page(thread_current()->pml4, page->va);
// }

static void file_backed_destroy(struct page *page) {
  // do_munmap(page->file.addr);
  if (page->va == 0)
    return;
  struct thread *curr = thread_current();
  struct supplemental_page_table *spt = &curr->spt;
  struct page *head_page;
  if (page->file.head_page == NULL) {
    head_page = page;
  } else
    head_page = page->file.head_page;
  struct file *file = head_page->file.file;

  void *upage = head_page->file.addr;
  // printf("문 페이지:%p", page);
  if (page == NULL)
    goto err;
  struct file_page *file_page = &page->file;

  size_t destroy_bytes = file_page->total_length;
  // printf("파일백 크기: %d\n", destroy_bytes);
  while (destroy_bytes > 0) {
    size_t page_destroy_bytes = destroy_bytes < PGSIZE ? destroy_bytes : PGSIZE;
    struct page *curr_page = spt_find_page(spt, upage);
    if (curr_page == NULL)
      curr_page = page;
    if (pml4_is_dirty(curr->pml4, upage)) {
      file_seek(file, curr_page->file.offset);
      file_write(file, page->va, page_destroy_bytes);
    }
    pml4_clear_page(curr->pml4, upage);
    palloc_free_page(curr_page->frame->kva);
    curr_page->va = 0;
    destroy_bytes -= page_destroy_bytes;
    upage += PGSIZE;
  }
  file_close(file);
err:
  return;
}

/* Do the mmap */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset) {
  // 주소나 길이가 NULL인 경우 처리되지 않도록 함
  if (addr == NULL || length == NULL)
    return false;

  // 현재 스레드 및 보조 페이지 테이블에 대한 참조를 설정
  struct thread *curr = thread_current();
  struct supplemental_page_table *spt = &curr->spt;

  // 주소를 페이지 경계로 반올림하여 초기화
  uint8_t *rd_page = pg_round_down(addr);

  // 파일에서 읽기를 시작할 위치 및 읽어야 할 전체 바이트 수 초기화
  off_t read_start = offset;
  uint32_t read_bytes = length;

  while (read_bytes > 0) {

    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;

    // lazy_mmap 구조체를 동적으로 할당하여 초기화
    struct lazy_mmap *aux =
        (struct lazy_mmap *)calloc(1, sizeof(struct lazy_mmap));
    if (aux == NULL) {
      return false;
    }

    // lazy_mmap 구조체 필드 초기화
    aux->length = length;
    aux->writable = writable;
    aux->upage = rd_page;
    aux->addr = addr;

    aux->offset = read_start;
    aux->page_read_bytes = page_read_bytes;

    // 파일 페이지를 추적하기 위해 보조 페이지 테이블에서 해당 가상 주소에 대한
    // 페이지 검색
    aux->head_page = spt_find_page(spt, addr);
    // head_page가 없으면 첫 페이지이므로 파일 핸들 설정
    if (aux->head_page == NULL)
      aux->file = file_reopen(file);

    read_start += page_read_bytes;

    // 가상 메모리에 페이지를 할당하고 초기화 함수(lazy_load_segment)를 전달하여
    // 페이지 생성
    if (!vm_alloc_page_with_initializer(VM_FILE, rd_page, writable,
                                        do_lazy_mmap, aux))
      return NULL;

    // 남은 바이트 수 및 파일 오프셋 업데이트
    read_bytes -= page_read_bytes;
    rd_page += PGSIZE;
  }
  return addr;
}

/* Do the munmap */
void do_munmap(void *addr) {
  struct thread *curr = thread_current();
  struct supplemental_page_table *spt = &curr->spt;

  // 주어진 가상 주소에 해당하는 페이지를 보조 페이지 테이블에서 찾습니다.
  struct page *page = spt_find_page(spt, addr);

  // 페이지의 시작 가상 주소를 `upage`로 설정합니다.
  void *upage = page->file.addr;
  // 파일 핸들을 결정합니다. 만약 `head_page`가 없으면 `file`을 사용하고,
  // 그렇지 않으면 `head_page`의 `file`을 사용합니다.
  struct file *file = page->file.head_page == NULL
                          ? page->file.file
                          : spt_find_page(spt, upage)->file.file;
  // 페이지와 관련된 `file_page` 구조체에 대한 포인터를 가져옵니다.
  struct file_page *file_page = &page->file;

  // 페이지를 해제할 바이트 수를 설정합니다.
  size_t destroy_bytes = file_page->total_length;

  while (destroy_bytes > 0) {
    // 페이지를 해제할 바이트 수를 페이지 크기 이하로 설정합니다.
    size_t page_destroy_bytes = destroy_bytes < PGSIZE ? destroy_bytes : PGSIZE;
    // 현재 페이지를 보조 페이지 테이블에서 가상 주소 `upage`로 찾습니다.
    struct page *curr_page = spt_find_page(spt, upage);

    // 현재 페이지에 프레임이 할당되지 않았고, VM의 유형이 `VM_UNINIT`이 아닌
    // 경우, 파일 정보를 설정하고 페이지를 할당합니다.
    if (curr_page->frame == NULL &&
        VM_TYPE(curr_page->operations->type) != VM_UNINIT) {
      curr_page->file.file = file_reopen(file);
      // 페이지를 할당합니다. (`VM_ANON`은 물리 메모리에 할당된 페이지를
      // 가리키는 상수입니다.)
      vm_alloc_page(VM_ANON, pg_round_down(curr_page->va), true);
      // 페이지를 클레임합니다.
      vm_claim_page(curr_page->va);
    }

    // 페이지가 더티한 경우 파일에 쓰고 파일 위치를 업데이트합니다.
    if (pml4_is_dirty(curr->pml4, upage)) {
      // 파일 위치를 설정합니다.
      file_seek(file, curr_page->file.offset);
      // 페이지를 파일에 쓰고 파일 위치를 업데이트합니다.
      file_write(file, curr_page->va, page_destroy_bytes);
    }
    // 페이지를 페이지 테이블에서 제거하고 프레임을 해제하며 페이지 엔트리를
    // 해시에서 제거합니다.
    pml4_clear_page(curr->pml4, upage);
    palloc_free_page(curr_page->frame->kva);
    hash_delete(&curr->spt.hash_pt, &curr_page->hash_elem);
    free(curr_page);

    // 남은 바이트 수를 업데이트하고 `upage`를 다음 페이지로 이동합니다.
    destroy_bytes -= page_destroy_bytes;
    upage += PGSIZE;
  }
  file_close(file);
}

/* do_lazy_mmap 함수는 파일 데이터를 가상 메모리 페이지로 로드하는 역할을
 * 합니다. 이 함수는 페이지 테이블의 page에 대한 정보와 aux 구조체를
 * 사용하여 파일 데이터를 읽어와 가상 메모리 페이지로 매핑합니다. */
bool *do_lazy_mmap(struct page *page, void *aux) {

  if (page == NULL)
    return false;

  struct thread *curr = thread_current();
  struct lazy_mmap *lm = (struct aux *)aux;

  // 파일 핸들 설정 (head_page가 NULL인 경우 파일 핸들을 사용하고 그렇지 않으면
  // head_page 파일 사용)
  struct file *file = page->file.head_page == NULL
                          ? page->file.file
                          : page->file.head_page->file.file;

  bool writable = lm->writable;
  off_t ofs = lm->offset;
  if (ofs <= file_length(file)) {
    // 파일 오프셋 이동 및 페이지 읽어오기 위한 준비
    file_seek(file, ofs);

    uint32_t page_read_bytes = lm->page_read_bytes;
    uint8_t *kpage = page->frame->kva;
    if (kpage == NULL)
      return false;

    // 파일 데이터를 읽어와 가상 메모리 페이지에 복사
    file_read(file, kpage, page_read_bytes);
  }
  // 사용한 aux 메모리를 해제
  free(aux);
  return true;
}