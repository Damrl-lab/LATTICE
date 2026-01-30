#include "slabInt.h"
#include "cont.h"
#include "atomics.h"
#include "page_alloc.h"
#include "pointer_locations.h"

static void index_slab_entry(unsigned int cid, struct slab_dir *sd, struct slab_entry *se)
{
    struct slab_entry_size *es;

    if (SLAB_ENTRY_IS_INIT(se)) {
        struct slab_entry_size key = { .es_size = se->se_size };
        es = RB_FIND(sizes_slab_entry_tree, &sd->sd_size_root, &key);
        if (es) {
            //TODO: consider caching the size to avoid calling slab_entry_full again. It's slow!
            if (slab_entry_full(se))
                STAILQ_INSERT_TAIL(&es->es_list, se, se_list);
            else
                STAILQ_INSERT_HEAD(&es->es_list, se, se_list);
        } else {
            es = slab_entry_size_init(se->se_size);
            RB_INSERT(sizes_slab_entry_tree, &sd->sd_size_root, es);
            STAILQ_INSERT_TAIL(&es->es_list, se, se_list);
        }
        RB_INSERT(used_slab_entry_tree, &sd->sd_maddr_root, se);
    } else {
        STAILQ_INSERT_TAIL(&sd->sd_free_list, se, se_list);
    }
}

static struct slab_bucket *slab_bucket_map(unsigned int cid, struct slab_dir *sd, size_t laddr, int type, int indexing)
{
    struct slab_bucket *sb;
    struct slab_entry *se;
    int i;

    sb = page_allocator_mappage_with_prot(cid, laddr,PROT_WRITE);
    page_allocator_mprotect(cid, sb, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);



    for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
        se = &sb->sb_entries[i];
        if (SLAB_ENTRY_IS_INIT(se)) {
            if (type == CPOINT_INCOMPLETE) {
                se->se_data.current.maddr = page_allocator_mappage(cid, se->se_data.current.laddr);
                se->se_data.snapshot.maddr = page_allocator_mappage(cid, se->se_data.snapshot.laddr);

            } else if (type == CPOINT_COMPLETE) {
              //todo ashikee:  Temporariliy commented. need to check again.  is it correct line 55,61??
//              assert(se->se_data.current.laddr == se->se_data.snapshot.laddr && "Inconsistent state on slab_entry data");

                se->se_data.current.maddr = page_allocator_mappage(cid, se->se_data.current.laddr);
                if(se->se_data.current.laddr == se->se_data.snapshot.laddr)
                {
                    se->se_data.snapshot.maddr = se->se_data.current.maddr;
                    se->se_data.snapshot.laddr = se->se_data.current.laddr;
                }
                else
                    se->se_data.snapshot.maddr = page_allocator_mappage(cid, se->se_data.snapshot.laddr);
                

              //todo ashikee:  Temporariliy commented. need to check again.
 //              assert(se->se_ptr.current.laddr == se->se_ptr.snapshot.laddr && "Inconsistent state on slab_entry ptrs");
               
            } else
                handle_error("invalid restore type\n");
        }

        if (indexing)
            index_slab_entry(cid, sd, se);
    }

    return sb;
}


static struct slab_inner* slab_inner_map(unsigned int cid, struct slab_dir *sd, size_t laddr, int type)
{
    struct slab_inner *si;
    int i;

    si = page_allocator_mappage_with_prot(cid, laddr,PROT_WRITE);
    page_allocator_mprotect(cid, si, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);


    for (i = 0; i < si->si_index; i++) {
        if (NOT_CS_CONSISTENT(si->si_current[i].laddr,  si->si_snapshot[i].laddr))
            handle_error("found inconsistent state while restoring inner\n");

        if (si->si_current[i].laddr) {
            if (type == CPOINT_INCOMPLETE) {
                //todo ashikee: temp disabled indexing. for segmentation error. needs to fixed.
               //si->si_current[i].maddr = slab_bucket_map(cid, sd, si->si_current[i].laddr, type, 1);
                si->si_current[i].maddr = slab_bucket_map(cid, sd, si->si_current[i].laddr, type, 1);
                si->si_snapshot[i].maddr = slab_bucket_map(cid, sd, si->si_snapshot[i].laddr, type, 1);
            } else if (type == CPOINT_COMPLETE) {
                //si->si_current[i].maddr = slab_bucket_map(cid, sd, si->si_snapshot[i].laddr, type, 1);
                si->si_current[i].maddr = slab_bucket_map(cid, sd, si->si_snapshot[i].laddr, type, 1);
                si->si_snapshot[i].maddr = si->si_current[i].maddr;
                si->si_current[i].laddr = si->si_snapshot[i].laddr;
            } else
                handle_error("invalid restore type\n");
        }
    }

    return si;
}


static struct slab_outer* slab_outer_map(unsigned int cid, struct slab_dir *sd, size_t laddr, int type)
{
    struct slab_outer *so;
    int i;

    so = page_allocator_mappage_with_prot(cid, laddr,PROT_WRITE);
    page_allocator_mprotect(cid, so, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);


    for (i = 0; i < so->so_index; i++) {
        if (NOT_CS_CONSISTENT(so->so_current[i].laddr, so->so_snapshot[i].laddr))
            handle_error("found inconsistent state while restoring outer\n");

        if (so->so_current[i].laddr) {
            if (type == CPOINT_INCOMPLETE) {
                so->so_current[i].maddr = slab_inner_map(cid, sd, so->so_current[i].laddr, type);
                so->so_snapshot[i].maddr = slab_inner_map(cid, sd, so->so_snapshot[i].laddr, type);
            } else if (type == CPOINT_COMPLETE) {
                so->so_current[i].maddr = slab_inner_map(cid, sd, so->so_snapshot[i].laddr, type);
                so->so_snapshot[i].maddr = so->so_current[i].maddr;
                so->so_current[i].laddr = so->so_snapshot[i].laddr;
            } else
                handle_error("invalid restore type\n");
        }
    }
    return so;
}

void map_pointer_pages(unsigned int cid, struct slab_dir* sd, int type)
{
    if(type == CPOINT_INCOMPLETE)
    {
        sd->pointer_locations_page.maddr = page_allocator_mappage_with_prot(cid, sd->pointer_locations_page.laddr,PROT_WRITE);
        page_allocator_mprotect(cid, sd->pointer_locations_page.maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
        struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;
        
        for(int i = 0; i< POINTER_LOCATIONS_PAGES_SIZE; i++)
        {
            if(plp->pointer_pages[i].laddr)
            {
                    plp->pointer_pages[i].maddr = page_allocator_mappage_with_prot(cid, plp->pointer_pages[i].laddr,PROT_WRITE);
                    plp->pointer_pages_ss[i].maddr = page_allocator_mappage_with_prot(cid, plp->pointer_pages_ss->laddr,PROT_WRITE);
                    page_allocator_mprotect(cid, plp->pointer_pages[i].maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
                    page_allocator_mprotect(cid, plp->pointer_pages_ss[i].maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);

            }
            else break;
        }
        sd->snapshot_pointer_locations_page.laddr = page_allocator_mappage_with_prot(cid,sd->snapshot_pointer_locations_page.laddr,PROT_WRITE);

    }
    else if(type == CPOINT_COMPLETE)
    {
        sd->pointer_locations_page.maddr = page_allocator_mappage_with_prot(cid, sd->snapshot_pointer_locations_page.laddr,PROT_WRITE);
        sd->snapshot_pointer_locations_page.maddr = sd->pointer_locations_page.maddr;
        sd->pointer_locations_page.laddr = sd->snapshot_pointer_locations_page.laddr;
        struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;
        page_allocator_mprotect(cid, plp, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);

        for(int i = 0; i< POINTER_LOCATIONS_PAGES_SIZE; i++)
        {
            if(plp->pointer_pages[i].laddr)
            {
                    plp->pointer_pages[i].maddr = page_allocator_mappage_with_prot(cid, plp->pointer_pages_ss[i].laddr,PROT_WRITE);
                    plp->pointer_pages_ss[i].maddr = plp->pointer_pages_ss[i].maddr;
                    plp->pointer_pages[i].laddr = plp->pointer_pages_ss[i].laddr;
                    page_allocator_mprotect(cid, plp->pointer_pages[i].maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
                    page_allocator_mprotect(cid, plp->pointer_pages_ss[i].maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);

                
            }
            else break;
        }
    }

}

struct slab_dir* slab_map(unsigned int cid, size_t laddr, int type)
{
    struct slab_dir *sd;
    int i;
    sd = page_allocator_mappage_with_prot(cid, laddr,PROT_WRITE);
    page_allocator_mprotect(cid, sd, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    VECTOR_INIT(&sd->sd_vector);
      //todo: ashikee temp fix 
    RB_INIT(&sd->sd_size_root);
    STAILQ_INIT(&sd->sd_free_list);

//    RB_REINIT(&sd->sd_maddr_root);
//    RB_REINIT(&sd->sd_size_root);
//    STAILQ_INIT(&sd->sd_free_list);

    for (i = 0; i < sd->sd_index; i++) {
        if (NOT_CS_CONSISTENT(sd->sd_current[i].laddr, sd->sd_snapshot[i].laddr))
            handle_error("found inconsistent state while restoring dir\n");

        if (sd->sd_current[i].laddr) {
            if (type == CPOINT_INCOMPLETE) {
                sd->sd_current[i].maddr = slab_outer_map(cid, sd, sd->sd_current[i].laddr, type);
                sd->sd_snapshot[i].maddr = slab_outer_map(cid, sd, sd->sd_snapshot[i].laddr, type);
            } else if (type == CPOINT_COMPLETE) {
                sd->sd_current[i].maddr = slab_outer_map(cid, sd, sd->sd_snapshot[i].laddr, type);
                sd->sd_snapshot[i].maddr = sd->sd_current[i].maddr;
                sd->sd_current[i].laddr = sd->sd_snapshot[i].laddr;
            } else
                handle_error("invalid restore type\n");
        }
    }
    //Fixing pointer pages
    map_pointer_pages(cid, sd,  type);
    return sd;
}
