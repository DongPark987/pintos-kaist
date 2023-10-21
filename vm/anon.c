/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
/* 익명 페이지를 파괴합니다. 페이지 자체는 호출자에 의해 해제될 것입니다. */
static void
anon_destroy (struct page *page) {
  // 페이지의 익명 페이지 구조체를 가져옵니다.
//   struct anon_page *anon_page = &page->anon;
  
//   // 페이지와 관련된 프레임이 존재하는 경우, 프레임의 페이지 포인터를 NULL로
//   // 설정합니다.
//   if (page->frame)
//     page->frame->page = NULL;
//   free(page->frame);
  struct thread *curr = thread_current();
  struct anon_page *anon_page = &page->anon;
  if (page->frame != NULL) {
    pml4_clear_page(curr->pml4, page->va);
    palloc_free_page(page->frame->kva);
  }
}
