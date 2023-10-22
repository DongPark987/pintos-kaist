/* vm.c: Generic interface for virtual memory objects. */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include <string.h>
#include "hash.h"

struct list vm_frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
	list_init(&vm_frame_table);
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
									bool writable, vm_initializer *init,
									void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;
	struct thread *curr = thread_current();

	/* Check wheter the upage is already occupied or not. */
	uint8_t *rd_upage = pg_round_down(upage);

	if (spt_find_page(spt, rd_upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */

		/* TODO: 페이지를 생성하고 VM 유형에 따라 초기화기를 가져오십시오.
		 * TODO: 그런 다음 uninit_new를 호출하여 "미초기화" 페이지 구조체를
		 * 생성하십시오.
		 * TODO: uninit_new를 호출한 후에 필드를 수정해야 합니다. */

		/* TODO: 페이지를 spt에 삽입하십시오. */

		struct page *t_page = (struct page *)calloc(1, sizeof(struct page));
		if (t_page == NULL)
			goto err;
		switch (type)
		{
		case VM_UNINIT:
			/* code */
			uninit_new(t_page, rd_upage, init, type, aux, NULL);
			break;
		case VM_ANON:
			/* code */
			uninit_new(t_page, rd_upage, init, type, aux, anon_initializer);
			t_page->writable = writable;
			break;
		case VM_FILE:
			/* code */
			uninit_new(t_page, rd_upage, init, type, aux, file_backed_initializer);
			t_page->writable = writable;
			break;
		case VM_PAGE_CACHE:
			/* code */
			uninit_new(t_page, rd_upage, init, type, aux, NULL);
			break;

		default:
			break;
		}
		// uninit_new(t_page, rd_upage, init, type, aux, anon_initializer);
		hash_insert(&spt->hash_pt, &t_page->hash_elem);
		return true;
	}
	else
		goto err;
	return true;

err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED,
						   void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function. */
	uint8_t *rd_upage = pg_round_down(va);

	page = page_lookup(rd_upage, spt);
	// if(page!=NULL){
	// 	printf("%p의 해시 찾음%p\n",va,page->va);
	// }

	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	int succ = true;
	hash_insert(&spt->hash_pt, &page->hash_elem);
	/* TODO: Fill this function. */

	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	if (!list_empty(&vm_frame_table))
	{
		victim = list_entry(list_pop_front(&vm_frame_table), struct frame, elem);
	}

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	// printf("희생자 찾음:%p\n", victim->kva);
	swap_out(victim->page);
	memset(victim->kva, 0, PGSIZE);
	// printf("희생자 :%p\n", victim->elem);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = malloc(sizeof(struct frame));
	if (frame == NULL)
		return NULL;
	uint8_t *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	frame->kva = kpage;
	frame->page = NULL;
	frame->link_cnt = 0;
	if (frame->kva == NULL)
	{
		free(frame);
		frame = vm_evict_frame();
		// printf("압수끝: %p\n",&frame->elem);
		if (frame == NULL)
		{
			return NULL;
		}
	}
	// printf("푸시백전\n");
	list_push_back(&vm_frame_table, &frame->elem);
	// printf("푸시백\n");
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED)
{
	vm_claim_page(addr);
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED,
						 bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct thread *curr = thread_current();
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	uint8_t *upage = pg_round_down(addr);
	// printf("스택탑: %p\n", spt->stack_top);
	// printf("폴트 발생 %p\n", addr);
	page = spt_find_page(&curr->spt, upage);
	// printf("찾았나%p\n", page);
	// printf("rsp:%p, addr: %p\n",f->rsp,addr);

	if (f == NULL)
	{
		// printf("%s %p 왔니, 타입: %d\n", curr->name, upage, page->operations->type);

		// if (page->frame->link_cnt == 0)
		// {
		// 	pml4_set_page(curr->pml4, page->va, page->frame->kva, page->writable);
		// }
		// else
		// {
			struct frame *copy_frame;
			copy_frame = vm_get_frame();
			memcpy(copy_frame->kva, page->frame->kva, PGSIZE);
			pml4_clear_page(curr->pml4, page->va);
			page->frame->link_cnt--;
			page->frame = copy_frame;
			copy_frame->page = page;
			pml4_set_page(curr->pml4, page->va, copy_frame->kva, page->writable);
		// }

		return true;
	}

	if (upage > USER_STACK - (1 << 20) && upage <= USER_STACK && f->rsp == addr)
	{
		// printf("스택 증가\n");
		vm_stack_growth(addr);
		return true;
	}

	if (addr < USER_STACK && addr > KERN_BASE)
		return false;

	if (page != NULL)
	{
		if (page->writable == true && write == true && page->frame != NULL && VM_TYPE(page->operations->type) != VM_UNINIT)
		{
			// printf("%s %p 왔니, 타입: %d\n", curr->name, upage, page->operations->type);
			// if (page->frame->link_cnt == 0)
			// {
			// 	pml4_set_page(curr->pml4, page->va, page->frame->kva, page->writable);
			// }
			// else
			// {
				struct frame *copy_frame;
				copy_frame = vm_get_frame();
				memcpy(copy_frame->kva, page->frame->kva, PGSIZE);
				pml4_clear_page(curr->pml4, page->va);
				page->frame->link_cnt--;
				page->frame = copy_frame;
				copy_frame->page = page;
				pml4_set_page(curr->pml4, page->va, copy_frame->kva, page->writable);
			// }

			return true;
		}
		if (page->writable == false && write == true)
			return false;
	}
	else
	{
		return false;
	}

	// // printf("aaa:%p\n", upage);
	// uint64_t *pte = pml4e_walk(curr->pml4, addr, 0);
	// // printf("%p,qaa\n",pte);
	// // if(pte != NULL && !is_writable(pte))
	// 	// printf("못쓴다.\n");
	// if (pte != NULL && !is_writable(pte) && write)
	// {
	// 	return false;
	// }
	// printf("aaa\n");
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	uint8_t *rd_upage = pg_round_down(va);
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	uint8_t *page = NULL;
	page = spt_find_page(spt, rd_upage);
	if (page == NULL)
	{
		if (!vm_alloc_page(VM_ANON, rd_upage, true))
			return false;
		page = spt_find_page(spt, rd_upage);
		if (page == NULL)
			return false;
	}

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	struct thread *curr = thread_current();
	pml4_set_page(curr->pml4, page->va, frame->kva, page->writable);

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	return swap_in(page, frame->kva);
}

unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->hash_pt, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED)
{
	struct hash_iterator i;
	struct thread *curr = thread_current();
	struct page *copy_page = NULL;
	struct frame *copy_frame = NULL;

	hash_first(&i, &src->hash_pt);
	while (hash_next(&i))
	{
		struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);

		if (is_kernel_vaddr(p->va))
			continue;

		copy_page = malloc(sizeof(struct page));
		if (copy_page == NULL)
			goto err;
		memcpy(copy_page, p, sizeof(struct page));
		/* lazy한 페이이지 인지 아닌지 구분 */
		if (p->frame != NULL)
		{
			// copy_frame = vm_get_frame();
			// if (copy_frame == NULL)
			// 	goto err;
			// memcpy(copy_frame->kva, p->frame->kva, PGSIZE);


			// 개선 예정
			// copy_frame = calloc(1, sizeof(struct frame));
			// copy_frame->kva = p->frame->kva;

			copy_frame = p->frame;
			p->frame->link_cnt++;
			copy_page->frame = copy_frame;
			copy_frame->page = copy_page;
			if (page_get_type(p) == VM_FILE)
			{
				copy_page->file.file = file_duplicate(p->file.file);
			}
			/* cow를 위한 부모 페이지 readonly 변경 */
			pml4_set_page(src->spt_pml4, p->va, p->frame->kva, false);
			/* cow를 위한 자식 페이지 readonly 변경 */
			pml4_set_page(curr->pml4, copy_page->va, p->frame->kva, false);
			// pml4_set_page(curr->pml4, copy_page->va, copy_frame->kva, p->writable);
			// printf("페이지 복사해서 넣는다. %p\n", new_page->va);
		}
		else
		{
			/* 다른 fork 된 프로세스가 먼저 lazy_load 할 경우 aux가 free 되는 경우를 방지  */
			size_t aux_size = 0;
			switch (p->uninit.type)
			{
			case VM_UNINIT:
				/* code */

				break;
			case VM_ANON:
				/* code */
				// aux_size = sizeof(struct lazy_file);
				struct lazy_file *anon_lazy_file = malloc(sizeof(struct lazy_file));
				if (anon_lazy_file == NULL)
					goto err;
				memcpy(anon_lazy_file, p->uninit.aux, sizeof(struct lazy_file));
				copy_page->uninit.aux = anon_lazy_file;

				break;
			case VM_FILE:
				/* code */
				// aux_size = sizeof(struct lazy_mmap);
				struct lazy_mmap *original_mmap_lazy = p->uninit.aux;
				struct lazy_mmap *mmap_lazy_file = malloc(sizeof(struct lazy_mmap));
				if (mmap_lazy_file == NULL)
					goto err;
				memcpy(mmap_lazy_file, original_mmap_lazy, sizeof(struct lazy_mmap));
				if (original_mmap_lazy->file != NULL)
					mmap_lazy_file->file = file_duplicate(original_mmap_lazy->file);
				copy_page->uninit.aux = mmap_lazy_file;

				break;
			case VM_PAGE_CACHE:
				/* code */
				break;

			default:
				break;
			}
			// lazy_file = malloc(aux_size);
			// if (lazy_file == NULL)
			// 	goto err;
			// memcpy(lazy_file, p->uninit.aux, aux_size);
		}
		spt_insert_page(dst, copy_page);
	}
	return true;
err:
	va_exeception_free(copy_page);
	return false;
}

void hash_free_page(struct hash_elem *h_page, void *aux UNUSED)
{
	struct page *page = hash_entry(h_page, struct page, hash_elem);
	vm_dealloc_page(page);

	// destroy(page);
	// free(page);
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */

	hash_clear(&spt->hash_pt, hash_free_page);
	// hash_destroy(&spt->hash_pt, hash_free_page);
}

struct page *
page_lookup(const void *address, struct supplemental_page_table *spt)
{
	struct page p;
	struct hash_elem *e;
	struct thread *curr = thread_current();

	p.va = address;

	e = hash_find(&spt->hash_pt, &p.hash_elem);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

bool va_set_page(void *va, void *kva, bool writable)
{
	struct thread *curr = thread_current();
	if (pml4_set_page(curr->pml4, va, kva, writable) == NULL)
		return false;
	return true;
}

void va_exeception_free(void *f)
{
	if (f != NULL)
		free(f);
}