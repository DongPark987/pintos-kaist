/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "bitmap.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "vm/vm.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk* swap_disk;
static bool anon_swap_in(struct page* page, void* kva);
static bool anon_swap_out(struct page* page);
static void anon_destroy(struct page* page);

struct bitmap* used_sector;
static int total_swap_page;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
    swap_disk = disk_get(1, 1);
    used_sector = bitmap_create(disk_size(swap_disk));
    // total_swap_page = disk_size(swap_disk) / 8;
}

/* Initialize the file mapping */
bool anon_initializer(struct page* page, enum vm_type type, void* kva)
{
    /* Set up the handler */
    page->operations = &anon_ops;

    struct anon_page* anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
/* 페이지를 파일에 기록하여 페이지를 교체합니다. */
static bool anon_swap_in(struct page* page, void* kva)
{
    struct anon_page* anon_page = &page->anon;
    struct thread* curr = thread_current();
    disk_sector_t sector = page->anon.sector;

    if (swap_disk == -1)
        return false; // 사용 가능한 스왑 슬롯이 없음

    // 페이지 내용을 스왑 디스크에서 읽어 페이지를 교체합니다.
    for (int i = 0; i < 8; i++) {
        disk_read(swap_disk, page->anon.sector + i, page->frame->kva + (i * DISK_SECTOR_SIZE));
    }
    // 사용한 섹터를 비트맵에서 해제합니다.
    bitmap_set_multiple(used_sector, page->anon.sector, 8, 0);
    anon_page->sector = NULL;
    // 페이지 테이블 업데이트
    pml4_set_page(curr->pml4, page->va, kva, page->writable);
    return true;
}

/* Swap out the page by writing contents to the swap disk. */
/* 페이지 내용을 스왑 디스크로 쓰면서 페이지를 교체합니다. */
static bool anon_swap_out(struct page* page)
{
    struct anon_page* anon_page = &page->anon;
    struct thread* curr = thread_current();
    // 빈 섹터를 찾아서 할당합니다.
    disk_sector_t free_sector = bitmap_scan_and_flip(used_sector, 0, 8, false);
    // 페이지 내용을 스왑 디스크에 기록
    for (int i = 0; i < 8; i++) {
        disk_write(swap_disk, free_sector + i, page->frame->kva + (i * DISK_SECTOR_SIZE));
    }
    anon_page->sector = free_sector;
    // 페이지 테이블 업데이트
    pml4_clear_page(curr->pml4, page->va);
    page->frame->page = NULL;
    page->frame = NULL;

    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
/* 익명 페이지를 파괴합니다. 페이지 자체는 호출자에 의해 해제될 것입니다. */
static void
anon_destroy(struct page* page)
{
    struct thread* curr = thread_current();
    struct anon_page* anon_page = &page->anon;
    if (page->frame != NULL) {
        pml4_clear_page(curr->pml4, page->va);
        palloc_free_page(page->frame->kva);
    }
}
