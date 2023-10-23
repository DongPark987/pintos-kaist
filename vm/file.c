/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

static bool file_backed_swap_in(struct page* page, void* kva);
static bool file_backed_swap_out(struct page* page);
static void file_backed_destroy(struct page* page);
static void do_file_backed_destroy(struct page* page, bool spt_hash_delete);

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
bool file_backed_initializer(struct page* page, enum vm_type type, void* kva)
{
    /* Set up the handler */
    page->operations = &file_ops;
    struct lazy_mmap* lm = page->uninit.aux;
    page->file.addr = lm->addr;
    page->file.file = lm->file;
    page->file.page_length = lm->page_read_bytes;
    page->file.total_length = lm->length;
    page->file.upage = lm->upage;
    page->file.offset = lm->offset;
    page->file.head_page = lm->head_page;
    struct file_page* file_page = &page->file;
    return true;
}

/* Swap in the page by read contents from the file. */
/* 파일에서 내용을 읽어 페이지를 교체합니다. */
static bool file_backed_swap_in(struct page* page, void* kva)
{
    struct thread* curr = thread_current();
    struct file* file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

    off_t offset = page->file.offset;
    bool writable = page->writable;
    // 4. 파일 오프셋이 파일 길이보다 작거나 같은 경우 페이지 스왑 인 가능
    if (page->file.offset <= file_length(file)) {
        size_t page_read_bytes = page->file.page_length;
        uint8_t* kpage = page->frame->kva;
        if (kpage == NULL)
            return false;
        file_read_at(file, kpage, page_read_bytes, offset);
    }
    return true;
}

/* Swap out the page by writeback contents to the file. */
/* 페이지를 파일에 기록하여 페이지를 교체합니다. */
static bool file_backed_swap_out(struct page* page)
{
    struct thread* curr = thread_current();
    // 스왑아웃될 페이지의 파일을 결정하는 데 사용하는 구문
    struct file* file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

    // 페이지가 변경되었으면 파일로 쓰기 작업 수행
    if (pml4_is_dirty(curr->pml4, page->va)) {
        file_write_at(file, page->frame->kva, page->file.page_length, page->file.offset);
        pml4_set_dirty(curr->pml4, page->va, 0);
    }
    // 페이지 테이블에서 페이지 매핑 제거
    pml4_clear_page(curr->pml4, page->va);
    // 프레임과 페이지 간의 연결 해제, 페이지의 프레임 정보 초기화
    page->frame->page = NULL;
    page->frame = NULL;
    return true;
}

static void file_backed_destroy(struct page* page)
{
    do_file_backed_destroy(page, false);
}

/* Do the mmap */
void* do_mmap(void* addr, size_t length, int writable, struct file* file, off_t offset)
{
    // 주소나 길이가 NULL인 경우 처리되지 않도록 함
    if (addr == NULL || length == NULL)
        return false;

    // 현재 스레드 및 보조 페이지 테이블에 대한 참조를 설정
    struct thread* curr = thread_current();
    struct supplemental_page_table* spt = &curr->spt;

    // 주소를 페이지 경계로 반올림하여 초기화
    uint8_t* rd_page = pg_round_down(addr);

    // 파일에서 읽기를 시작할 위치 및 읽어야 할 전체 바이트 수 초기화
    off_t read_start = offset;
    uint32_t read_bytes = length;

    while (read_bytes > 0) {

        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;

        // lazy_mmap 구조체를 동적으로 할당하여 초기화
        struct lazy_mmap* aux = (struct lazy_mmap*)calloc(1, sizeof(struct lazy_mmap));
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
        if (!vm_alloc_page_with_initializer(VM_FILE, rd_page, writable, do_lazy_mmap, aux))
            return NULL;

        // 남은 바이트 수 및 파일 오프셋 업데이트
        read_bytes -= page_read_bytes;
        rd_page += PGSIZE;
    }
    return addr;
}

/* Do the munmap */
void do_munmap(void* addr)
{
    struct thread* curr = thread_current();
    struct supplemental_page_table* spt = &curr->spt;
    // 주어진 가상 주소에 해당하는 페이지를 보조 페이지 테이블에서 찾습니다.
    struct page* page = spt_find_page(spt, addr);
    do_file_backed_destroy(page, true);
}

/* do_lazy_mmap 함수는 파일 데이터를 가상 메모리 페이지로 로드하는 역할을
 * 합니다. 이 함수는 페이지 테이블의 page에 대한 정보와 aux 구조체를
 * 사용하여 파일 데이터를 읽어와 가상 메모리 페이지로 매핑합니다. */
bool* do_lazy_mmap(struct page* page, void* aux)
{

    if (page == NULL)
        return false;

    struct thread* curr = thread_current();
    struct lazy_mmap* lm = (struct aux*)aux;

    // 파일 핸들 설정 (head_page가 NULL인 경우 파일 핸들을 사용하고 그렇지 않으면 head_page 파일 사용)
    struct file* file = page->file.head_page == NULL ? page->file.file : page->file.head_page->file.file;

    bool writable = lm->writable;
    off_t ofs = lm->offset;
    if (ofs <= file_length(file)) {
        // 파일 오프셋 이동 및 페이지 읽어오기 위한 준비
        file_seek(file, ofs);

        uint32_t page_read_bytes = lm->page_read_bytes;
        uint8_t* kpage = page->frame->kva;
        if (kpage == NULL)
            return false;

        // 파일 데이터를 읽어와 가상 메모리 페이지에 복사
        file_read(file, kpage, page_read_bytes);
    }
    // 사용한 aux 메모리를 해제
    free(aux);
    return true;
}

static void do_file_backed_destroy(struct page* page, bool spt_hash_delete)
{
    struct thread* curr = thread_current();
    struct supplemental_page_table* spt = &curr->spt;
    void* upage = page->file.addr;
    struct file* file = page->file.file != NULL ? page->file.file : page->file.head_page->file.file;

    /* va가 0인경우 이미 자원정리가 완료된 페이지로 판별 */
    if (page->va == 0)
        return;
    struct file_page* file_page = &page->file;

    ASSERT(pg_ofs(upage) == 0);
    size_t destroy_bytes = file_page->total_length;
    while (destroy_bytes > 0) {
        size_t page_destroy_bytes = destroy_bytes < PGSIZE ? destroy_bytes : PGSIZE;
        struct page* curr_page = spt_find_page(spt, upage);
        if (curr_page == NULL)
            curr_page = page;

        if (curr_page->frame != NULL) {
            if (pml4_is_dirty(curr->pml4, upage))
                file_write_at(file, curr_page->va, page_destroy_bytes, curr_page->file.offset);

            pml4_clear_page(curr->pml4, upage);

            palloc_free_page(curr_page->frame->kva);
            free(curr_page->frame);
        }

        /* Advance. */
        /* munmap인 경우에만 해시 테이블 제거 */
        if (spt_hash_delete) {
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