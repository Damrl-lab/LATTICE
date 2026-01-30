#ifndef FIXMAPPER_H
#define FIXMAPPER_H

#include <stdlib.h>
#include <stdint.h>

#include "page_alloc.h"
#include "cont.h"
#define MAX_FREE_FIXED_PAGES    ((PAGE_SIZE -sizeof(uint64_t)) / \
                                ( sizeof(uint32_t)))





struct fixed_page;
struct fixed_mapper;
struct free_fixed_pages
{
    uint16_t page_number[MAX_FREE_FIXED_PAGES];
    uint64_t next_free_page;

};
struct fixed_page *fixed_page_alloc(uint64_t pgno);
void fixed_page_free(struct fixed_page *p);
void *map_hint(size_t len);
static void update_mapping_prot(struct fixed_mapper *fm);
static void fixed_mapper_grow_file(struct fixed_mapper *fm, uint64_t new_size);
static void map_file(struct fixed_mapper *fm, int update_mappings);
void *fixed_mapper_init();
int fixed_mapper_close(void *handler);
void *fixed_mapper_alloc_page(void *handler, size_t *laddr, int flags);
void *fixed_mapper_alloc_pages(void *handler, size_t n, size_t *laddr, int flags);
void *fixed_mapper_alloc_contiguous_pages(void *handler, size_t n, size_t *laddr, int flags);
void fixed_mapper_freepages(void *handler, void *maddr);
void *fixed_mapper_getaddress(void *handler, size_t laddr);
void fixed_mapper_noope(void *handler);
void fixed_mapper_noswap(void *handler, void *xaddr, size_t ypgoff, void *yaddr, size_t xpgoff);
void copy_file_mapper_to_container(void *handler, void* cont);
struct page_allocator_ops *fixed_mapper_ops();


#endif /* end of include guard: FIXMAPPER_H */
