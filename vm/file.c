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
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
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
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	// printf("스왑인 파일 페이지: %p\n",page->va);
	// struct file *file = lm->file;
	struct file *file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

	off_t ofs = page->file.offset;
	bool writable = page->writable;
	if (ofs <= file_length(file))
	{
		file_seek(file, ofs);

		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = page->file.page_length;
		uint8_t *kpage = page->frame->kva;

		if (kpage == NULL)
			return false;

		file_read(file, kpage, page_read_bytes);
		/* Load this page. */
	}

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	// printf("아웃 %p\n", page->va);
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	// struct file *file = page->file.head_page == NULL ? page->file.file : spt_find_page(spt, page->va)->file.file;
	struct file *file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

	if (pml4_is_dirty(curr->pml4, page->va))
	{
		file_seek(file, page->file.offset);
		file_write(file, page->va, page->file.page_length);
	}
	pml4_clear_page(curr->pml4, page->va);
	page->frame->page = NULL;
	page->frame = NULL;
	// printf("아웃 성공\n");
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	// do_munmap(page->file.addr);
	if (page->va == 0)
		return;
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	struct page *head_page;
	if (page->file.head_page == NULL)
	{
		head_page = page;
	}
	else
		head_page = page->file.head_page;

	struct file *file = head_page->file.file;

	void *upage = head_page->file.addr;
	// printf("문 페이지:%p", page);
	if (page == NULL)
		goto err;
	struct file_page *file_page = &page->file;

	size_t destroy_bytes = file_page->total_length;
	// printf("파일백 크기: %d\n", destroy_bytes);
	while (destroy_bytes > 0)
	{
		size_t page_destroy_bytes = destroy_bytes < PGSIZE ? destroy_bytes : PGSIZE;
		struct page *curr_page = spt_find_page(spt, upage);
		if (curr_page == NULL)
			curr_page = page;
		// printf("upage: %p, kva: %p\n", curr_page->va, curr_page->frame->kva);

		// if (curr_page->frame == NULL || VM_TYPE(curr_page->operations->type) != VM_UNINIT)
		// {
		// 	// printf("요청한다: %p.\n", curr_page->va);
		// 	curr_page->file.file = file;
		// 	vm_claim_page(curr_page->va);
		// }
		// if (true)
		// // if (true)
		// {

		// }
		if (curr_page->frame != NULL)
		{
			if (pml4_is_dirty(curr->pml4, upage))
			{
				file_seek(file, curr_page->file.offset);
				file_write(file, curr_page->va, page_destroy_bytes);
			}
			pml4_clear_page(curr->pml4, upage);
			if (curr_page->frame->link_cnt == 0)
			{
				palloc_free_page(curr_page->frame->kva);
				free(curr_page->frame);
			}
			else
			{
				curr_page->frame->link_cnt--;
			}
		}
		curr_page->va = 0;
		destroy_bytes -= page_destroy_bytes;
		upage += PGSIZE;
	}
	file_close(file);
err:
	return;
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	void *upage = pg_round_down(addr);

	size_t read_bytes = length;
	off_t ofs_curr = offset;
	while (read_bytes > 0)
	{
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		struct lazy_mmap *aux = calloc(1, sizeof(struct lazy_mmap));

		// 파일백드의 시작주소
		aux->addr = addr;
		aux->upage = upage;
		aux->length = length;
		aux->writable = writable;

		aux->offset = ofs_curr;
		aux->page_read_bytes = page_read_bytes;
		aux->head_page = spt_find_page(spt, addr);
		if (aux->head_page == NULL)
			aux->file = file;
		ofs_curr += page_read_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, upage,
											writable, do_lazy_mmap, aux))
			return NULL;

		/* Advance. */
		read_bytes -= page_read_bytes;
		upage += PGSIZE;
	}

	return addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{

	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	struct page *page = spt_find_page(spt, addr);
	void *upage = page->file.addr;
	struct file *file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

	// struct file *file = page->file.head_page == NULL ? page->file.file : spt_find_page(spt, upage)->file.file;
	if (page == NULL)
		goto err;
	struct file_page *file_page = &page->file;

	// printf("문맵시작%p\n",addr);
	ASSERT(pg_ofs(upage) == 0);
	size_t destroy_bytes = file_page->total_length;
	while (destroy_bytes > 0)
	{
		size_t page_destroy_bytes = destroy_bytes < PGSIZE ? destroy_bytes : PGSIZE;
		struct page *curr_page = spt_find_page(spt, upage);
		// spt_remove_page()
		if (curr_page == NULL)
			goto err;

		// if (curr_page->frame == NULL && VM_TYPE(curr_page->operations->type) != VM_UNINIT)
		// {
		// 	curr_page->file.file = file;
		// 	vm_claim_page(curr_page->va);
		// }

		// if (pml4_is_dirty(curr->pml4, upage))
		// {
		// 	file_seek(file, curr_page->file.offset);
		// 	file_write(file, curr_page->va, page_destroy_bytes);
		// }
		// pml4_clear_page(curr->pml4, upage);
		// palloc_free_page(curr_page->frame->kva);
		if (curr_page->frame != NULL)
		{
			if (pml4_is_dirty(curr->pml4, upage))
			{
				file_seek(file, curr_page->file.offset);
				file_write(file, curr_page->va, page_destroy_bytes);
			}
			pml4_clear_page(curr->pml4, upage);
			if (curr_page->frame->link_cnt == 0)
			{
				palloc_free_page(curr_page->frame->kva);
				free(curr_page->frame);
			}
			else
			{
				curr_page->frame->link_cnt--;
			}
		}

		/* Advance. */
		// printf("만든다 엠멥\n");
		hash_delete(&curr->spt.hash_pt, &curr_page->hash_elem);
		free(curr_page);
		destroy_bytes -= page_destroy_bytes;
		upage += PGSIZE;
	}
	file_close(file);
err:
	return;
}

bool *do_lazy_mmap(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	struct lazy_mmap *lm = aux;
	// printf("레이지 파일 페이지: %p\n",page->va);
	// struct file *file = lm->file;
	struct file *file = page->file.head_page == NULL ? page->file.file : page->file.head_page->file.file;

	off_t ofs = lm->offset;
	bool writable = lm->writable;
	// if (ofs <= file_length(file))
	// {
	// 	file_seek(file, ofs);

	/* Do calculate how to fill this page.
	 * We will read PAGE_READ_BYTES bytes from FILE
	 * and zero the final PAGE_ZERO_BYTES bytes. */
	size_t page_read_bytes = lm->page_read_bytes;
	uint8_t *kpage = page->frame->kva;

	if (kpage == NULL)
		return false;
	file_read_at(file, kpage, page_read_bytes, ofs);
	// file_read(file, kpage, page_read_bytes);
	/* Load this page. */
	// }

	free(aux);
	return true;
}
