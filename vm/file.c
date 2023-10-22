/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);
static void do_file_backed_destroy(struct page *page, bool spt_hash_delete);

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
	struct file_page *file_page = &page->file;
	struct lazy_mmap *lm = page->uninit.aux;
	file_page->addr = lm->addr;
	file_page->file = lm->file;
	file_page->page_length = lm->page_read_bytes;
	file_page->total_length = lm->length;
	file_page->upage = lm->upage;
	file_page->offset = lm->offset;
	file_page->head_page = lm->head_page;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	struct file *file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

	off_t offset = page->file.offset;
	bool writable = page->writable;

	/* 오프셋이 파일의 전체 길이보다 작은 경우만 read */
	if (offset <= file_length(file))
	{
		file_seek(file, offset);
		size_t page_read_bytes = page->file.page_length;
		uint8_t *kpage = page->frame->kva;
		if (kpage == NULL)
			return false;
		file_read_at(file, kpage, page_read_bytes, offset);
	}

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	// struct file *file = page->file.head_page == NULL ? page->file.file : spt_find_page(spt, page->va)->file.file;
	struct file *file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

	/* 더티비트가 체크되어 있는 경우에만 write */
	if (pml4_is_dirty(curr->pml4, page->va))
	{
		file_seek(file, page->file.offset);
		file_write(file, page->va, page->file.page_length);
	}
	pml4_clear_page(curr->pml4, page->va);
	page->frame->page = NULL;
	page->frame = NULL;
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	do_file_backed_destroy(page, false);
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

	do_file_backed_destroy(page, true);
}

static void do_file_backed_destroy(struct page *page, bool spt_hash_delete)
{
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
	void *upage = page->file.addr;
	struct file *file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

	/* va가 0인경우 이미 자원정리가 완료된 페이지로 판별 */
	if (page->va == 0)
		return;
	struct file_page *file_page = &page->file;

	ASSERT(pg_ofs(upage) == 0);
	size_t destroy_bytes = file_page->total_length;
	while (destroy_bytes > 0)
	{
		size_t page_destroy_bytes = destroy_bytes < PGSIZE ? destroy_bytes : PGSIZE;
		struct page *curr_page = spt_find_page(spt, upage);
		if (curr_page == NULL)
			curr_page = page;

		if (curr_page->frame != NULL)
		{
			if (pml4_is_dirty(curr->pml4, upage))
				file_write_at(file, curr_page->va, page_destroy_bytes, curr_page->file.offset);

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
		/* munmap인 경우에만 해시 테이블 제거 */
		if (spt_hash_delete)
		{
			hash_delete(&curr->spt.hash_pt, &curr_page->hash_elem);
			free(curr_page);
		}
		/* spt_kill을 통해 순회 제거중인 경우 임의로 페이지를 제거하지 않고 va값만 0으로 변경 */
		else
			curr_page->va = 0;
			
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
	struct file *file = page->file.head_page == NULL ? page->file.file : page->file.head_page->file.file;

	off_t ofs = lm->offset;
	bool writable = lm->writable;

	/* Do calculate how to fill this page.
	 * We will read PAGE_READ_BYTES bytes from FILE
	 * and zero the final PAGE_ZERO_BYTES bytes. */
	size_t page_read_bytes = lm->page_read_bytes;
	uint8_t *kpage = page->frame->kva;

	if (kpage == NULL)
		return false;
	file_read_at(file, kpage, page_read_bytes, ofs);

	free(aux);
	return true;
}
