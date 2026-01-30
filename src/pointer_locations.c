#include "pointer_locations.h"
#include "slabInt.h"
#include "macros.h"
#include "cont.h"
#include "stats.h"
#include "atomics.h"
#include "htable.h"
#include "closure.h"
#include "page_alloc.h"


#define PTR_REBASE(base, ploc, newbase) ((newbase) + ((ploc) - (base)))


struct pointer_locations_page* pointer_locations_page_snapshot(unsigned int cid, struct 
pointer_locations_page *plp, size_t *laddr)
{
    struct pointer_locations_page *plps = NULL;
    size_t snapshot_laddr;

    STATS_INC_COWMETA();

    plps = (struct pointer_locations_page*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
    if (plps == NULL)
        handle_error("failed to allocate memory for slab_dir snapshot\n");
    pmemcpy(plps, plp, ROUNDPG(sizeof(*plp)));
    atomic_set(laddr, snapshot_laddr);
    return plps;
}

struct pointer_page* pointer_page_snapshot(unsigned int cid, struct 
pointer_page *pp, size_t *laddr)
{
    struct pointer_page *pps = NULL;
    size_t snapshot_laddr;

    STATS_INC_COWMETA();

    pps = (struct pointer_page*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
    if (pps == NULL)
        handle_error("failed to allocate memory for slab_dir snapshot\n");
    pmemcpy(pps, pp, ROUNDPG(sizeof(*pp)));
    atomic_set(laddr, snapshot_laddr);
    return pps;
}


int delete_pointer_location(int cid, size_t pointer_location)
{
    struct slab_dir *sd;
    sd =  get_container(cid)->current_slab.maddr;
    unsigned long total_entries = sd->total_pointers_size;

    if(!sd->pointer_locations_page.maddr)
    return -1;

    struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;


    int i = 0;
    int j = 0;
    for( i = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j; i++)
    {
        struct pointer_page* page = (struct pointer_page*) 
            plp->pointer_pages[i].maddr;

        for(int j = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j ; j++)
        {
            if(page->pointer_laddr[j] == pointer_location)
            {
                struct pointer_page* page_last = (struct pointer_page*) 
                plp->pointer_pages[total_entries/POINTER_ENTRIES_SIZE].maddr;
                if(plp->pointer_pages[total_entries/POINTER_ENTRIES_SIZE].laddr ==
                plp->pointer_pages_ss[total_entries/POINTER_ENTRIES_SIZE].laddr)
                     plp->pointer_pages_ss[total_entries/POINTER_ENTRIES_SIZE].maddr =
                      pointer_page_snapshot(cid, plp->pointer_pages[total_entries/POINTER_ENTRIES_SIZE].laddr, 
                     &plp->pointer_pages_ss[total_entries/POINTER_ENTRIES_SIZE].laddr);

                if(plp->pointer_pages[i].laddr ==
                plp->pointer_pages_ss[i].laddr)
                     plp->pointer_pages_ss[i].maddr =
                      pointer_page_snapshot(cid, plp->pointer_pages[i].maddr, 
                     &plp->pointer_pages_ss[i].laddr);

                size_t temp =  page->pointer_laddr[j];
                int pos_in_last_page = total_entries%POINTER_ENTRIES_SIZE - 1;
                page->pointer_laddr[j] = page_last->pointer_laddr
                [pos_in_last_page];
                page_last->pointer_laddr
                [pos_in_last_page] = temp;

                sd->total_pointers_size--;

                return 1;

            }
        }
        j = 0;
    }
    return -1;

}
int insert_pointer_location(int cid, size_t pointer_location, unsigned int pval_slab_entry, uint16_t pval_offset)
{
    struct slab_dir *sd;
    sd =  get_container(cid)->current_slab.maddr;
    unsigned long total_entries = sd->total_pointers_size;

    // create new pointer location page if not allocated
    if(!sd->pointer_locations_page.maddr)
    {
        page_allocator_mprotect(cid, sd, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
        sd->pointer_locations_page.maddr = (struct pointer_locations_page*)
                page_allocator_getpage(cid, &sd->pointer_locations_page.laddr,
                 PA_PROT_WRITE);
    }
        

    // create new snapshot pointer location page if not allocated
    if(!sd->snapshot_pointer_locations_page.laddr ||
        sd->snapshot_pointer_locations_page.laddr == sd->pointer_locations_page.laddr)
        sd->snapshot_pointer_locations_page.maddr = pointer_locations_page_snapshot(cid, sd->pointer_locations_page.maddr, 
        &sd->snapshot_pointer_locations_page.laddr);
    


    if(total_entries >= POINTER_LOCATIONS_PAGES_SIZE * POINTER_ENTRIES_SIZE) 
        return -1; //All pointer pages allocated, allocation failed

    struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;
    
    //Todo: ashikee optimize insert, search takes long time
    int i = 0;
    int j = 0;
    for( i = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j; i++)
    {
        struct pointer_page* page = (struct pointer_page*) 
            plp->pointer_pages[i].maddr;

        for(int j = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j ; j++)
        {
            if(page->pointer_laddr[j] == pointer_location)
            {
                page->pointer_laddr[j] = pointer_location;
                page->pval_slab_entry[j] = pval_slab_entry;
                page->pval_offset[j] = pval_offset;
                return;
            }
        }
        j = 0;
    }

    int pointer_page_number = total_entries/  POINTER_ENTRIES_SIZE;

    int position_in_pointer_page = total_entries % POINTER_ENTRIES_SIZE;

    if(!plp->pointer_pages[pointer_page_number].maddr)
    {
        //allocate new pointer page
        plp->pointer_pages[pointer_page_number].maddr = 
         (void* ) page_allocator_getpage(cid,  &plp->pointer_pages[pointer_page_number].laddr, PA_PROT_WRITE);
    }
    if(plp->pointer_pages[pointer_page_number].laddr ==
        plp->pointer_pages_ss[pointer_page_number].laddr)
    {
        //snapshot pointer page
        plp->pointer_pages_ss[pointer_page_number].maddr = 
        pointer_page_snapshot(cid, sd->pointer_locations_page.maddr, 
        &plp->pointer_pages_ss[pointer_page_number].laddr);

    }

    struct pointer_page* page = (struct pointer_page*) plp->pointer_pages[pointer_page_number].maddr;

    page->pointer_laddr[position_in_pointer_page] = pointer_location;
    page->pval_slab_entry[position_in_pointer_page] = pval_slab_entry;
    page->pval_offset[position_in_pointer_page] = pval_offset;
    sd->total_pointers_size++;
    return 0;
}

void test_insert(int cid, size_t laddr, unsigned int pval_slab_entry, uint16_t pval_offset)
{
    int i = 0;

    for(i = 0; i< 515; i++)
        insert_pointer_location(cid, laddr + i, pval_slab_entry, pval_offset);

}

void test_delete(int cid, size_t laddr)
{
    delete_pointer_location(cid, laddr + 5);

}

int insert_pointer_locations_in_ht_pointerat(int cid)
{
    struct slab_dir *sd;
    sd =  get_container(cid)->current_slab.maddr;
    unsigned long total_entries = sd->total_pointers_size;

    if(!sd->pointer_locations_page.maddr)
    return -1;

    struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;


    int i = 0;
    int j = 0;
    for( i = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j; i++)
    {
        struct pointer_page* page = (struct pointer_page*) 
            plp->pointer_pages[i].maddr;
        for(int j = 0; total_entries > ((i* POINTER_ENTRIES_SIZE)+j) && j < POINTER_ENTRIES_SIZE  ; j++)
        {
            htable_insert_2(get_ht_pointerat(),
            page_allocator_mappage_with_prot(cid,page->pointer_laddr[j],PROT_WRITE ) ,
            page_allocator_mappage_with_prot(cid,page->pointer_laddr[j],PROT_WRITE ),
            2);
        }
        j = 0;
    }
    return 1;

}

void print_all_pointer_locations(int cid)
{
    printf("\n\n Stored Pointer Locations in PM:\n\n");
    struct slab_dir *sd;
    sd =  get_container(cid)->current_slab.maddr;
    unsigned long total_entries = sd->total_pointers_size;
    if(!sd->pointer_locations_page.maddr)
    return -1;

    struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;
    int i = 0;
    int j = 0;
    for( i = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j; i++)
    {
        struct pointer_page* page = (struct pointer_page*) 
            plp->pointer_pages[i].maddr;

        for(int j = 0; total_entries > (i* POINTER_ENTRIES_SIZE)+j ; j++)
        {
            void* addr = page_allocator_mappage_with_prot(cid,page->pointer_laddr[j],PROT_WRITE);
            printf("%p %u %u\n ",addr,(unsigned int) page->pval_slab_entry,
             (unsigned int) page->pval_offset );
        }
        j = 0;
    }
    return 1;
}
