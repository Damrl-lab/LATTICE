#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>

#include "cont.h"
#include "out.h"
#include "settings.h"
#include "macros.h"
#include "slab.h"
#include "sfhandler.h"
#include "page_alloc.h"
#include "atomics.h"
#include "container_version.h"
#include "pointer_locations.h"
/**
 * This array contains pointer to the actual containers. There is fixed numbers
 * of containers
 */
struct container * CONTAINERS[CONTAINER_CNT] = { NULL };
int restoring = 0;
int* cpoint_phase;
static int container_getid()
{
    int i;

    for (i = 0; i < CONTAINER_CNT; i++) {
        if (CONTAINERS[i] == NULL)
           return i;
    }

    return -1;
}

static void dont_compute_closure(unsigned int cid) { }

static void compute_closure(unsigned int cid)
{
    /*
     * TODO: with this implementation, we could get duplicates on the
     * persistent list of persistent pointers. This shold not affect
     * correctness, but it should be fixed!
     */
    LOG(10, "Computing closure");
    build_mallocat_tree();
    classify_pointers(cid);
    move_volatile_allocations(cid);
    fix_back_references();
    empty_mallocat_tree();
    insert_pointer_locations_in_ht_pointerat(cid);
    //print_all_pointer_locations(cid);
}

static void (*Func_compute_closure)(unsigned int cid) = compute_closure;

/*
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void init(void)
{
    out_init(PMLIB_LOG_PREFIX, PMLIB_LOG_LEVEL_VAR, PMLIB_LOG_FILE_VAR,
            PMLIB_MAJOR_VERSION, PMLIB_MINOR_VERSION);
    LOG(3, NULL);

    register_sigsegv_handler();

    char *ptr = getenv("PMLIB_CLOSURE");
    if (ptr) {
        int val = atoi(ptr);
        if (val == 0) {
            Func_compute_closure = dont_compute_closure;
            LOG(3, "Container closure has been disabled");
        }
    } else {
        Func_compute_closure = compute_closure;
        LOG(3, "Container closure is enabled");
    }

    slab_init();
}

struct container *container_init()
{
    int cid = container_getid();
    return container_init_cid(cid);

}

struct container *container_initialization(int cid)
{
    return container_init_cid(cid);

}

struct container *container_init_cid(int cid)
{
     //Todo: Atomic operations to set values
    struct container *cont;
    struct page_allocator *pallocator;
    //TODO: handle the case of multiple containers
    size_t cont_laddr;
    pallocator = page_allocator_init(cid);
    cont = page_allocator_getpage(cid, &cont_laddr, PA_PROT_WRITE);
    cont->is_not_head = 0;
    if (!cont)
        handle_error("failed to allocate memory for container\n");
    if (cont_laddr != CONTAINER_LIMA_ADDRESS)
        handle_error("failed to get the expected laddr for container\n");

    closure_init();

    CONTAINERS[cid] = cont;
    cont->id = cid;
    cont->pg_allocator = pallocator;
    cont->current_slab.maddr = slab_dir_init(cid, &cont->current_slab.laddr);
    cont->snapshot_slab.maddr = cont->current_slab.maddr;
    cont->snapshot_slab.laddr = cont->current_slab.laddr;
    cont->total_number_of_free_pages = 0;
    cont->version_dir_index = 0;

    //Initializing version directory. All the available versions can be accessed from here
    cont->current_version_dir.maddr = version_dir_init(cid,
     &cont->current_version_dir.laddr );
    cont->snapshot_version_dir.maddr = cont->current_version_dir.maddr;
    cont->snapshot_version_dir.laddr = cont->current_version_dir.laddr;

    cont->fixed_pages.maddr = page_allocator_getpage(cid, &cont->fixed_pages.laddr, PA_PROT_WRITE);
    cont->snapshot_fixed_pages.maddr = cont->fixed_pages.maddr;
    cont->snapshot_fixed_pages.laddr = cont->fixed_pages.laddr;
    return cont;
}

// Allocates memory directly from the persistent memory
void *container_palloc(unsigned int cid, unsigned int size)
{
    return slab_palloc(cid, size);
}

static void container_compute_closure(unsigned int cid)
{
    Func_compute_closure(cid);
}


static void container_cpoint_aux(unsigned int cid, int type, int is_async)
{
    struct container *cont;
    struct ptrat *pat, *pat_temp;
    cont = get_container(cid);
    page_allocator_mprotect(cid, cont, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    atomic_set_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS);
    container_compute_closure(cid);
    slab_cpoint(cid, type,is_async);


}
 int container_cpoint(unsigned int cid)
{
    LOG(10, "Starting a new checkpoint");
    container_cpoint_aux(cid, CPOINT_REGULAR,0);
    LOG(8, stats_pt_report());
    LOG(8, stats_general_report());
    STATS_RESET_TRANSACTION_COUNTERS();
    return get_allocated_pages_count();
}

 int container_checkpoint(unsigned int cid, int is_async)
{
    LOG(10, "Starting a new checkpoint");
    container_cpoint_aux(cid, CPOINT_REGULAR,is_async);
    LOG(8, stats_pt_report());
    LOG(8, stats_general_report());
    STATS_RESET_TRANSACTION_COUNTERS();
    container_set_checkpoint_phase(NO_CP_ONGOING_LIBVPM);
    return get_allocated_pages_count();
}

int container_checkpoint_with_phase(unsigned int cid, int is_async, int* checkpoint_phase)
{
    cpoint_phase = checkpoint_phase;
    return container_checkpoint(cid, is_async); 
}

void container_set_checkpoint_phase(int phase)
{
    *cpoint_phase = phase;
}

struct container *container_restore(unsigned int cid)
{
    container_restore_for_version(cid,MAX_UNSIGNED_INT);
}

//Restores container for a specific version id
struct container *container_restore_for_version(unsigned int cid, unsigned int version_id)
{

    restoring = 1;
    struct container *cont;
    struct page_allocator *pallocator;

    pallocator = get_current_page_allocator(cid);
    if(pallocator == 0)
        pallocator = page_allocator_init(cid);
    cont = page_allocator_mappage_with_prot(cid, CONTAINER_LIMA_ADDRESS,PROT_WRITE);
    CONTAINERS[cid] = cont;
    page_allocator_mprotect(cid, cont, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    cont->pg_allocator = pallocator;
     cont->current_version_dir.maddr = page_allocator_mappage_with_prot(
         cid,cont->current_version_dir.laddr,PROT_WRITE);
    cont->snapshot_version_dir.maddr = page_allocator_mappage_with_prot(
         cid,cont->snapshot_version_dir.laddr,PROT_WRITE);

    closure_init();

    if (test_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS)) {
        cont->current_slab.maddr = slab_map(cid, cont->current_slab.laddr, CPOINT_INCOMPLETE);

        if (cont->current_slab.laddr != cont->snapshot_slab.laddr)
            cont->snapshot_slab.maddr = slab_map(cid, cont->snapshot_slab.laddr, CPOINT_INCOMPLETE);
        container_cpoint_aux(cid, CPOINT_RESTORE,0);
        atomic_clear_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS);
    } else {
        //TODO: think about what should be atomic here
         if(version_id != MAX_UNSIGNED_INT)
        {
            cont->is_not_head = 1;
            struct version_entry* ve = get_version_entry_by_id(cont,cont->current_version_dir.maddr,version_id);
            cont->current_slab.maddr = slab_map(cid, ve->current_version_slab.laddr, CPOINT_COMPLETE);
            cont->snapshot_slab.maddr = slab_map(cid, ve->snapshot_version_slab.laddr, CPOINT_COMPLETE);
            cont->current_slab.laddr = ve->current_version_slab.laddr;
            cont->snapshot_slab.laddr = ve->snapshot_version_slab.laddr;
        }
        else
        {
            cont->is_not_head = 0;
            cont->current_slab.maddr = slab_map(cid, cont->head_slab.laddr, CPOINT_COMPLETE);
            cont->current_slab.laddr = cont->head_slab.laddr;
            cont->snapshot_slab.maddr = slab_map(cid, cont->head_snapshot_slab.laddr, CPOINT_COMPLETE);
            cont->snapshot_slab.laddr = cont->head_snapshot_slab.laddr;
        }
    }
    insert_pointer_locations_in_ht_pointerat(cid);
    page_allocator_mprotect(cid, cont->current_slab.maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    slab_fixptrs(cid);
    slab_mprotect_datapgs(cid, PROT_READ);
    register_sigsegv_handler();
    restoring = 0;
    return cont;
}

struct version_dir *get_version_dir(unsigned int cid)
{
    struct container *cont;
    struct page_allocator * pallocator;
    pallocator = get_current_page_allocator(cid);
    if(pallocator == 0)
        pallocator = page_allocator_init(cid);
    cont = page_allocator_mappage_with_prot(cid, CONTAINER_LIMA_ADDRESS,PROT_WRITE);
    CONTAINERS[cid] = cont;
    page_allocator_mprotect(cid, cont, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    return cont->current_version_dir.maddr;
}

size_t container_setroot(unsigned int cid, void *maddr)
{
    return slab_setroot(cid, maddr);
}

void *container_getroot(unsigned int cid)
{
    return slab_getroot(cid);
}

void container_test_pointer_insert(unsigned int cid, void* addr, unsigned int  se_id, 
uint16_t  offset)
{
   test_insert(cid, addr,se_id, offset);
}

void container_test_pointer_delete(unsigned int cid, void* addr)
{
   test_delete(cid, addr);
}

int is_restoring()
{
   return restoring;
}

