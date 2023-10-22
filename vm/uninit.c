/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */
/* uninit.c: 초기화되지 않은 페이지의 구현.
 *
 * 모든 페이지는 초기화되지 않은 페이지로 생성됩니다. 첫 번째 페이지 폴트가 발생할 때,
 * 핸들러 체인은 uninit_initialize (page->operations.swap_in)를 호출합니다.
 * uninit_initialize 함수는 페이지 객체 (anon, file, page_cache)로 변환하고,
 * 페이지 객체를 초기화하며 vm_alloc_page_with_initializer 함수에서 전달된
 * 초기화 콜백을 호출합니다. */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void uninit_new(struct page *page, void *va, vm_initializer *init,
				enum vm_type type, void *aux,
				bool (*initializer)(struct page *, enum vm_type, void *))
{
	ASSERT(page != NULL);

	*page = (struct page){
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page){
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}};
}

/* Initalize the page on first fault */
static bool
uninit_initialize(struct page *page, void *kva)
{
	struct uninit_page *uninit = &page->uninit;

	// printf("언이닛 실행\n");
	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	return uninit->page_initializer(page, uninit->type, kva) &&
		   (init ? init(page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy(struct page *page)
{
	struct uninit_page *uninit UNUSED = &page->uninit;
		// printf("언이닛 디스트로이 왔다.\n");

	if (uninit->type == VM_FILE)
	{
		// printf("언이닛 디스트로이 왔다.\n");
		// struct thread *curr = thread_current();
		// struct supplemental_page_table *spt = &curr->spt;
		// hash_insert(&curr->spt.hash_pt, &page->hash_elem);
		// struct lazy_mmap *lm = page->uninit.aux;
		// do_munmap(lm->addr);
	}

	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
