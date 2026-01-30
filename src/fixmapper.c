#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "page_alloc.h"
#include "settings.h"
#include "macros.h"
#include <queue.h>
#include "out.h"
#include "stats.h"
#include "cont.h"
#include "atomics.h"
#include "cont.h"
#include "sys_queue_listop.h"

#define GIGABYTE    (1UL << 30)
#define GIGABYTE_MASK (GIGABYTE - 1)
#define ROUND2GB(x) (((x)+GIGABYTE_MASK)&~GIGABYTE_MASK)
#define TERABYTE    (1UL << 40)
#define PROCMAXLEN  2048

#define bytes2pgs(bytes) ((bytes) / PAGE_SIZE)

#define DEFAULT_MAPPING_PROT    (PA_PROT_READ)



char cont_file_name[128];

struct fixed_page {
    uint64_t pgno;
    int prot_flags;
    LIST_ENTRY(fixed_page) free;
};

struct pages_segment {

    size_t pgno;
    uint32_t page_count;
    int prot_flags;
    LIST_ENTRY(pages_segment) free;
};

struct fixed_page *fixed_page_alloc(uint64_t pgno)
{
    struct fixed_page *p = malloc(sizeof(*p));
    assert(p && "Failed to allocated memory for page");
    p->pgno = pgno;
    p->prot_flags = DEFAULT_MAPPING_PROT;
    return p;
}

struct pages_segment *pages_segment_alloc(uint64_t pgno, uint32_t page_count)
{
    struct pages_segment *p = malloc(sizeof(*p));
    assert(p && "Failed to allocated memory for page");
    p->pgno = pgno;
    p->page_count = page_count;
    p->prot_flags = DEFAULT_MAPPING_PROT;
    return p;
}


void fixed_page_free(struct fixed_page *p)
{
    assert(p && "Trying to free NULL pointer");
    free(p);
    p = NULL;
}

void pages_segment_free(struct pages_segment *p)
{
    //not implemented
    //assert(p && "Trying to free NULL pointer");
    //free(p);
    //p = NULL;
}

int cmpfunc (const void * a, const void * b) {
   return ( *(uint16_t*)a - *(uint16_t*)b );
}

struct fixed_mapper {
    int fd;             //file descriptor
    size_t file_size;   //file size in bytes
    void *start_addr;   //address at which the entire file is mapped
    struct {
        uint64_t size;
        LIST_HEAD(pages_segment_head, pages_segment) head;
        struct pages_segment *last;
    } free_list;                //keep track of all available pages
    int initialized;
};

int list_size(struct fixed_mapper* h){

    struct pages_segment *itr;
    int pg_index = 0;
    LIST_FOREACH(itr, &h->free_list.head,free )
        pg_index += itr->page_count;
    return pg_index;
}

/*
 * map_hint -- (internal) use /proc to determine a hint address for mmap()
 *
 * This is a helper function for util_map().  It opens up /proc/self/maps
 * and looks for the first unused address in the process address space that is:
 * - greater or equal 1TB,
 * - large enough to hold range of given length,
 * - 1GB aligned.
 *
 * Asking for aligned address like this will allow the DAX code to use large
 * mappings.  It is not an error if mmap() ignores the hint and chooses
 * different address.
 */
void *map_hint(size_t len)
{
    FILE *fp;
    if ((fp = fopen("/proc/self/maps", "r")) == NULL) {
        handle_error("!/proc/self/maps");
    }

    char line[PROCMAXLEN];  /* for fgets() */
    char *lo = NULL;    /* beginning of current range in maps file */
    char *hi = NULL;    /* end of current range in maps file */
    char *raddr = (char *)TERABYTE; /* ignore regions below 1TB */

    while (fgets(line, PROCMAXLEN, fp) != NULL) {
        /* check for range line */
        if (sscanf(line, "%p-%p", &lo, &hi) == 2) {
            if (lo > raddr) {
                if (lo - raddr >= len) {
                    break;
                } else {
                    //region is too small
                }
            }

            if (hi > raddr) {
                /* check the next range align to 1GB */
                raddr = (char *)ROUND2GB((uintptr_t)hi);
            }

            if (raddr == 0) {
                break;
            }
        }
    }

    /*
     * Check for a case when this is the last unused range in the address
     * space, but is not large enough. (very unlikely)
     */
    if ((raddr != NULL) && (UINTPTR_MAX - (uintptr_t)raddr < len)) {
        raddr = NULL;
    }

    fclose(fp);

    return raddr;
}

static void update_mapping_prot(struct fixed_mapper *fm)
{
    //todo-ashikee: disabled for now
    /*
    size_t pgs = bytes2pgs(fm->file_size);
    struct fixed_page *p = NULL;
    void *addr = NULL;

    for (size_t i = 0; i < pgs; i++) {
        p = fm->index[i];
        addr = fm->start_addr + PAGE_SIZE * p->pgno;
        page_allocator_mprotect_generic(addr, PAGE_SIZE, p->prot_flags);
    }
    */
}

static void fixed_mapper_grow_file(struct fixed_mapper *fm, uint64_t new_size)
{
    LOG(5, "Growing container file from %lu to %lu", fm->file_size, new_size);
    STATS_INC_CONTGROW();

    int rc = posix_fallocate(fm->fd, fm->file_size, new_size);
    switch (rc) {
        case 0: break;
        case EBADF  : handle_error("fd is not a valid file descriptor, or is not opened for writing.\n");
        case EFBIG  : handle_error("offset+len exceeds the maximum file size.\n");
        case EINVAL : handle_error("offset was less than 0, or len was less than or equal to 0.\n");
        case ENODEV : handle_error("fd does not refer to a regular file.\n");
        case ENOSPC : handle_error("There is not enough space left on the device containing the file referred to by fd.\n");
        case ESPIPE : handle_error("fd refers to a pipe.\n");
    }

    uint64_t current_size_pgs = bytes2pgs(fm->file_size);
    uint64_t new_size_pgs = bytes2pgs(new_size);
    struct pages_segment *p = NULL;

    p = pages_segment_alloc(current_size_pgs, new_size_pgs -current_size_pgs);
    if(fm->free_list.size == 0)
        LIST_INSERT_HEAD(&fm->free_list.head, p, free);
    else
         LIST_INSERT_AFTER(fm->free_list.last, p, free);
    fm->free_list.last = p;
    fm->free_list.size++;
    fm->file_size = new_size;
}

static void map_file(struct fixed_mapper *fm, int update_mappings)
{
    void *hint;

    if (fm->start_addr == 0)
        hint = map_hint(fm->file_size);
    else {
        hint = fm->start_addr;
        munmap(hint, fm->file_size);
    }
    assert(hint && "Could not find a big-enough region");

    void *addr = mmap(hint, fm->file_size, DEFAULT_MAPPING_PROT, MAP_SHARED, fm->fd, 0);
    assert(addr == hint && "Could not mapped at the hinted address");
    fm->start_addr = addr;

    if (update_mappings)
        update_mapping_prot(fm);
}

void *fixed_mapper_init()
{
    LOG(3, "Initializing fixed-mapper");
    int cid = 0;
    struct stat stat;
    struct fixed_mapper *fm = calloc(1, sizeof(*fm));
    char *ptr;

    assert(fm && "Failed to allocate memory");
    LIST_INIT(&fm->free_list.head);

    ptr = getenv("PMLIB_CONT_FILE");
    if (ptr) {
        LOG(3, "Setting container path to %s", ptr);
        sprintf(cont_file_name, "%s%d", ptr, cid);
    } else {
        sprintf(cont_file_name, "%s%d", FM_FILE_NAME_PREFIX, cid);
    }
    fm->fd = open(cont_file_name, O_CREAT | O_RDWR, 0777);
    if (fm->fd == -1)
        handle_error("fixed mapper failed to open file\n");

    fstat(fm->fd, &stat);
    fm->file_size = stat.st_size;

    if (fm->file_size == 0) {
        ptr = getenv("PMLIB_INIT_SIZE");
        //uint64_t file_size = FM_FILE_SIZE;
        uint64_t file_size ;
       
        if (ptr) {
            file_size = atoll(ptr);
            file_size = MAX(ROUNDPG(file_size), PAGE_SIZE);
            LOG(3, "Container init size has been set to %lu", file_size);
        }
        file_size = 4096*2*8;
        file_size *=1000000;
        fixed_mapper_grow_file(fm, file_size);
        map_file(fm, /* dont update mappings */ 0);
        fm->initialized = 1;

    }
    else
    {
        map_file(fm, /* dont update mappings */ 0);
        if(!fm->initialized) //applications restarted pages info are not available
        {
            fm->initialized = 1;
            uint64_t current_size_pgs = bytes2pgs(0);
            uint64_t new_size_pgs = bytes2pgs(fm->file_size);
            struct pages_segment *p = NULL;
            struct container* cont = fm->start_addr;
           

            struct free_pages_segment *ffp = fm->start_addr + cont->fixed_pages.laddr;
            int inserted_head = 0;
            uint32_t index_free_pages = 0;

            if(cont->total_number_of_free_pages)
            {
                for (uint64_t i = 0; i < cont->total_number_of_free_pages; i++) {
                      
                    p = pages_segment_alloc( ffp->pgno[index_free_pages],ffp->page_count[index_free_pages]);
                    if(inserted_head)
                    {
                        LIST_INSERT_AFTER(fm->free_list.last, p, free);
                        fm->free_list.size++;
                        fm->free_list.last = p;
                    }
                    else
                    {
                        LIST_INSERT_HEAD(&fm->free_list.head, p, free);
                        fm->free_list.last = p;
                        fm->free_list.size++;
                        inserted_head = 1;
                    }

                    index_free_pages++;
                       
                    if(index_free_pages == cont->total_number_of_free_pages)
                    {
                            index_free_pages = 0;
                            continue;
                    }
                    else
                    {
                        if(index_free_pages%MAX_FREE_PAGES_SEGMENT == 0)
                        {
                            if(ffp->next_free_pages_segment)
                            {
                                ffp =  fm->start_addr + ffp->next_free_pages_segment;
                            }
                        }
                    }    
                  }

             }    

        }
    }
    

    return (void*)fm;
}

int fixed_mapper_close(void *handler)
{
    //todo ashikee: not used
    /*
    struct fixed_mapper *h = (struct fixed_mapper*) handler;

    size_t size_pgs = bytes2pgs(h->file_size);
    for (size_t i = 0; i < size_pgs; ++i) {
        free(h->index[i]);
    }
    free(h->index);

    int ret = munmap(h->start_addr, h->file_size);
    assert(ret == 0 && "Failed to ummap the file");
    close(h->fd);
    */
    return 0;
}

void *fixed_mapper_alloc_page(void *handler, size_t *laddr, int flags)
{
    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    struct pages_segment *p;
    void *addr = NULL;    
    if (h->free_list.size == 0) {
        fixed_mapper_grow_file(h, h->file_size * 2);
        map_file(h, /* update mappings */ 1);
    }

    //printf("list size: %d %d \n",h->free_list.size, list_size(h));

    p = LIST_FIRST(&h->free_list.head);
    addr = h->start_addr + (p->pgno * PAGE_SIZE);
    if (laddr)
        *laddr = p->pgno * PAGE_SIZE;
    if (flags & PA_PROT_WRITE)
        page_allocator_mprotect_generic(addr, PAGE_SIZE, flags);

    p->pgno = p->pgno+1;
    p->page_count = p->page_count -1;
    
    if(p->page_count == 0)
    {
        LIST_REMOVE(p, free);
        h->free_list.size--;
        if (h->free_list.size == 0)
            h->free_list.last = NULL;
    }
    //LIST_PRINT( &h->free_list.head,  struct fixed_page, free, smpl_print );
    return addr;
}

void *fixed_mapper_alloc_pages(void *handler, size_t n, size_t *laddr, int flags)
{
    handle_error("not implemnted yet");
    return NULL;
}

void smpl_print( struct pages_segment* p ){
	printf("%d %d", p->pgno, p->page_count);
}
static inline int smpl_cmp( struct fixed_page* a, struct fixed_page* b ){
	return a->pgno - b->pgno;
}
void *fixed_mapper_alloc_contiguous_pages(void *handler, size_t n, size_t *laddr, int flags)
{

    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    struct pages_segment *p;
    void *addr = NULL;
    //printf("list size: %d %d \n",h->free_list.size, list_size(h));      
    //LIST_SORT(&h->free_list.head,  struct fixed_page, free, smpl_cmp );
    
    struct pages_segment *itr;


    while(1)
    {
        LIST_FOREACH(itr, &h->free_list.head,free ) {
            if(itr->page_count >= n)
            {
                addr = h->start_addr + (itr->pgno * PAGE_SIZE);
                
                if (laddr)
                    *laddr =itr->pgno * PAGE_SIZE;
                
                if (flags & PA_PROT_WRITE)
                    page_allocator_mprotect_generic(addr, PAGE_SIZE* n, flags);
                itr->page_count = itr->page_count - n;
                itr->pgno = itr->pgno+n;
    
        
                if(itr->page_count == 0)
                {
                    LIST_REMOVE(itr, free);
                    h->free_list.size--;
                    if (h->free_list.size == 0)
                        h->free_list.last = NULL;
                }

                return addr;

            }
        }
        fixed_mapper_grow_file(h, h->file_size * 2);
        map_file(h,  1);

    }

   //LIST_PRINT( &h->free_list.head,  struct fixed_page, free, smpl_print );
    return addr;

}

void fixed_mapper_freepages(void *handler, void *maddr)
{
    //todo: ashikee free pages size mismatches disabled temporarily.
   return;
   /*
    void *maddr_paligned = itop(ROUND_DWNPG(ptoi(maddr)));
    assert(maddr == maddr_paligned && "Address is not page aligned");

    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    uint64_t pgno = (maddr - h->start_addr) / PAGE_SIZE;
    struct fixed_page *p = h->index[pgno];

    if (h->free_list.last)
        LIST_INSERT_AFTER(h->free_list.last, p, free);
    else
        LIST_INSERT_HEAD(&h->free_list.head, p, free);

    h->free_list.last = p;
    h->free_list.size++;
    */
}

void *fixed_mapper_getaddress(void *handler, size_t laddr)
{
    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    void *maddr = h->start_addr + laddr;
    return maddr;
}

void *fixed_mapper_getaddress_setting_prot(void *handler, size_t laddr, int prot)
{
    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    /*
    int pg_no = laddr/PAGE_SIZE;
    struct fixed_page *p = NULL;
    p = h->index[pg_no];
    p->prot_flags = prot;
    */
    void *maddr = h->start_addr + laddr;
    return maddr;
    
}

void fixed_mapper_noope(void *handler)
{
    //nothing to do here!
}

void fixed_mapper_noswap(void *handler, void *xaddr, size_t ypgoff, void *yaddr, size_t xpgoff)
{
    handle_error("Fixed Mapper does not implement page mapping swap\n");
}


void copy_file_mapper_to_container(void *handler, void* cont)
{
    //todo: ashikee add snapshot pages for linkedlist of free_fixed_pages
    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    struct container* c = cont;
    struct pages_segment *itr;
    struct free_pages_segment *itr_free_fixed_pages;
    if(c->fixed_pages.maddr == c->snapshot_fixed_pages.maddr)
        c->fixed_pages.maddr= page_allocator_getpage(c->id,
         &c->fixed_pages.laddr, PA_PROT_WRITE);

    else 
        c->fixed_pages.maddr = page_allocator_mappage(c->id,c->fixed_pages.laddr);

    page_allocator_mprotect(c->id, c->fixed_pages.maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);

    uint16_t required_fixed_pages = h->free_list.size/MAX_FREE_PAGES_SEGMENT; 
    if(h->free_list.size%MAX_FREE_PAGES_SEGMENT)
        required_fixed_pages++;

    int already_available_fixed_pages = 0;
    if(c->total_number_of_free_pages > 0)
    {
        already_available_fixed_pages = c->total_number_of_free_pages/MAX_FREE_PAGES_SEGMENT;
        if(c->total_number_of_free_pages%MAX_FREE_PAGES_SEGMENT)
            already_available_fixed_pages++;
    }
    
  //  LIST_SORT(&h->free_list.head,  struct fixed_page, free, smpl_cmp );
    for(int i = 0; i< required_fixed_pages; i++)
    {
        if(i == 0)
        {
            itr_free_fixed_pages = c->fixed_pages.maddr;
        }
        else
        {
            if(i < already_available_fixed_pages)
            {
                itr_free_fixed_pages = page_allocator_mappage(c->id,
                itr_free_fixed_pages->next_free_pages_segment);
                page_allocator_mprotect(c->id,itr_free_fixed_pages, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);

            }
            else
            {
                 itr_free_fixed_pages= page_allocator_getpage(c->id,
                &itr_free_fixed_pages->next_free_pages_segment, PA_PROT_WRITE);   
                page_allocator_mprotect(c->id,itr_free_fixed_pages, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
            }
        }
    }

    c->total_number_of_free_pages = h->free_list.size;
    int i = 0;
    LIST_FOREACH(itr, &h->free_list.head,free ) {
        if(i% MAX_FREE_PAGES_SEGMENT == 0)
        {
            if(i == 0)
            {
                itr_free_fixed_pages = (struct free_fixed_pages*) c->fixed_pages.maddr;

            }
            else
            {
                itr_free_fixed_pages = page_allocator_mappage(c->id,
                itr_free_fixed_pages->next_free_pages_segment);

            }
        }
        itr_free_fixed_pages->pgno[i%MAX_FREE_PAGES_SEGMENT] = itr->pgno;
        itr_free_fixed_pages->page_count[i%MAX_FREE_PAGES_SEGMENT] = itr->page_count;
        i++;
    }
       //qsort(fp->page_number, fp->size, sizeof(uint16_t), cmpfunc);


}




struct page_allocator_ops *fixed_mapper_ops()
{
    static struct page_allocator_ops ops = {
        .name = "Fixed Mapper Page Allocation",
        .init = fixed_mapper_init,
        .copy_mapper_to_container = copy_file_mapper_to_container,
        .shutdown = fixed_mapper_close,
        .alloc_page = fixed_mapper_alloc_page,
        .alloc_pages = fixed_mapper_alloc_pages,
        .alloc_contiguous_pages = fixed_mapper_alloc_contiguous_pages,
        .free_pages = fixed_mapper_freepages,
        .map_page = fixed_mapper_getaddress,
        .map_page_with_prot = fixed_mapper_getaddress_setting_prot,
        .swap_page_mapping = fixed_mapper_noswap,
        .protect_page = page_allocator_mprotect_generic,
    };
    return &ops;
}

