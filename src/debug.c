
#include "slabInt.h"
#include "container_version.h"
#include "cont.h"
#include "fixmapper.h"
#include "pointer_locations.h"

void print_splay_tree()
{
    struct slab_entry *se;
    struct slab_dir *sd = get_container(0)->current_slab.maddr;
    int count = 0;
    RB_FOREACH(se, used_slab_entry_tree, &sd->sd_maddr_root) {
        printf("tree's slab_entrys [maddr: %p]\n", se->se_data.current.maddr);
        count++;
        if(count == 100)
            break;
    }
}

void slab_entry_print(unsigned int cid)
{
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_outer *so;
    struct slab_inner *si;
    struct slab_bucket *sb;
    struct slab_entry *se;
    int i, j, k, l;

    for (i = 0; i < sd->sd_index; i++) {
        so = sd->sd_current[i].maddr;
        for (j = 0; j < so->so_index; j++) {
            si = so->so_current[j].maddr;
            for (k = 0; k < si->si_index; k++) {
                sb = si->si_current[k].maddr;
                for (l = 0; l < SLAB_BUCKET_ENTRIES; l++) {
                    se = &sb->sb_entries[l];

                    if (!SLAB_ENTRY_IS_INIT(se))
                        continue;

                    printf("all slab_entrys [maddr: %p]\n", se->se_data.current.maddr);
                }
            }
        }
    }
}

int bitmap_set_count(bitstr_t *bm, int size)
{
    int count = 0;
    for (int i = 0; i < size; ++i) {
        count += bit_test(bm, i) ? 1 : 0;
    }
    return count;
}

void slab_entry_pprint(struct slab_entry *se, int level)
{
    int step = 2;
    printf("%*sse (%p) [id: %u, size: %u modified:%c]\n",
            level, "", se, se->se_id, se->se_size, se->modified);

    printf("%*sdata_c [maddr: %p, laddr: %zu bm: %d (%d)]\n", level + step, "",
            se->se_data.current.maddr,
            se->se_data.current.laddr,
            bitmap_set_count(SLAB_ENTRY_CBITMAP(se), SLAB_ENTRY_CAPACITY(se)),
            SLAB_ENTRY_CAPACITY(se));
    if (se->se_data.current.laddr != se->se_data.snapshot.laddr || 1)
        printf("%*sdata_s [maddr: %p, laddr: %zu bm: %d (%d)]\n", level + step, "",
                se->se_data.snapshot.maddr,
                se->se_data.snapshot.laddr,
                bitmap_set_count(SLAB_ENTRY_SBITMAP(se), SLAB_ENTRY_CAPACITY(se)),
                SLAB_ENTRY_CAPACITY(se));
    
}

void slab_bucket_pprint(struct slab_bucket *sb, int level)
{
    int step = 2;
    int i;
    printf("%*ssb (%p) [id: %u, capacity: %lu  modified:%c]\n",
            level, "", sb, sb->sb_id, SLAB_BUCKET_ENTRIES,sb->modified);
    for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
        if (sb->sb_entries[i].se_size)
            slab_entry_pprint(&sb->sb_entries[i], level + step);
    }
}

void slab_inner_pprint(struct slab_inner *si, int level)
{
    int step = 2;
    int i;
    struct slab_bucket *sbc, *sbs;
    printf("%*ssi (%p) [index: %u, capacity: %lu  modified:%c]\n",
            level, "", si, si->si_index, SLAB_INNER_ENTRIES,si->modified);
    for (i = 0; i < si->si_index; i++) {
        sbc = si->si_current[i].maddr;
        sbs = si->si_snapshot[i].maddr;
        slab_bucket_pprint(sbc, level + step);
        if (sbc != sbs)
            slab_bucket_pprint(sbs, level + step);
    }
}

void slab_outer_pprint(struct slab_outer *so, int level)
{
    int step = 2;
    int i;
    struct slab_inner *sic, *sis;
    printf("%*sso (%p) [index: %u, capacity: %lu] modified:%c\n",
            level, "", so, so->so_index, SLAB_OUTER_ENTRIES,so->modified);
    for (i = 0; i < so->so_index; i++) {
        sic = so->so_current[i].maddr;
        sis = so->so_snapshot[i].maddr;
        slab_inner_pprint(sic, level + step);
        if (sic != sis)
            slab_inner_pprint(sis, level + step);
    }
}

void slab_dir_pprint(struct slab_dir *sd, int level)
{
    int step = 2;
    int i;
    struct slab_outer *soc, *sos;
    printf("%*ssd (%p) [index: %u, capacity: %lu modified:%c] \n", level, "", sd, sd->sd_index, SLAB_DIR_ENTRIES,sd->modified);
    for (i = 0; i < sd->sd_index; i++) {
        soc = sd->sd_current[i].maddr;
        sos = sd->sd_snapshot[i].maddr;
        slab_outer_pprint(soc, level + step);
        if (soc != sos)
            slab_outer_pprint(sos, level + step);

          
    }

    struct pointer_locations_page*  plp = sd->pointer_locations_page.maddr;
    slab_pointers_pprint(plp, level + step);

    //pptr_print(&sd->sd_ptr_list_head);
}

slab_pointers_pprint(struct pointer_locations_page* plp, int level)
{
    for(int i = 0; i< POINTER_LOCATIONS_PAGES_SIZE && plp->pointer_pages[i].maddr ; i++)
    {
        struct pointer_page* pp = plp->pointer_pages[i].maddr;
        printf("%*spp (%p) [pointer page: %d] \n", level, "", pp, i);
        for(int j = 0; j< POINTER_ENTRIES_SIZE && pp->pointer_laddr[j] !=  0; j++)
            printf("%*spointer [index: %d, pointer_loc: %lu se_id:%lu offset:%lu] \n", level + 2, "", j, 
            pp->pointer_laddr[j], pp->pval_slab_entry[j],pp->pval_offset[j]);
    }
}

void version_dir_pprint(struct version_dir *vd, int level)
{
    printf("%*svd (%p) [capacity: %lu]\n",
            level, "", vd,VERSION_ENTRIES);
}

/*
void slab_dir_phead_ptr(struct slab_dir *sd, int level)
{
    int step;
    int i;
    struct pptr_node *node;

    pptr_for_each(&sd->sd_ptr_list_head, node) {
        printf("%*spnode (%p) [index: %u]\n", level, "", node, node->pn_index);
    }
}
*/

void container_pprint()
{
    int step = 2;
    struct container *cont = CONTAINERS[0];
    printf("cont (%p) [id: %u]\n", cont, cont->id);
    version_dir_pprint(cont->current_version_dir.maddr, step);
    if (cont->current_version_dir.maddr != cont->snapshot_version_dir.maddr)
        version_dir_pprint(cont->snapshot_version_dir.maddr, step);
    slab_dir_pprint(cont->current_slab.maddr, step);

    if (cont->current_slab.maddr != cont->snapshot_slab.maddr)
        slab_dir_pprint(cont->snapshot_slab.maddr, step);
            
}

void container_pprint_all_versions()
{
    int step = 2;
    struct container *cont = CONTAINERS[0];
    printf("cont (%p) [id: %u]\n", cont, cont->id);
    
    struct version_dir* vd = cont->current_version_dir.maddr;
    version_dir_pprint(cont->current_version_dir.maddr, step);
    if (cont->current_version_dir.maddr != cont->snapshot_version_dir.maddr)
        version_dir_pprint(cont->snapshot_version_dir.maddr, step);

    int current_index = 0;
    for(int i =0; i < cont->version_dir_index; i++)
    {
        if(i%VERSION_ENTRIES == 0 && i > current_index)
        {
            vd = vd->next_current_version_dir.maddr;
            current_index = i;
            if(!vd)
                break;
        }
        printf("version_id: %d timestamp: %u\n", vd->version_entries[i%VERSION_ENTRIES].version_id,
         vd->version_entries[i%VERSION_ENTRIES].timestamp);
            slab_dir_pprint(vd->version_entries[i%VERSION_ENTRIES].current_version_slab.maddr, step);
    }
    printf("Current Slab:\n");
    slab_dir_pprint(cont->current_slab.maddr, step);
    if (cont->current_slab.maddr != cont->snapshot_slab.maddr)
    {
        printf("Snapshot Slab:\n");
        slab_dir_pprint(cont->snapshot_slab.maddr, step);

    }            
}


void print_free_pages()
{
    struct container *cont = CONTAINERS[0];
    struct free_fixed_pages *fp = cont->fixed_pages.maddr;

    printf("\n Free Pages size: %d  Pages Number: ",cont->total_number_of_free_pages);
    for(int i = 0; i < cont->total_number_of_free_pages; i++)
    {
        if(i%MAX_FREE_FIXED_PAGES == 0)
        {
            if(i != 0)
            fp = fp->next_free_page;
        }
        printf("%d ",fp->page_number[i%MAX_FREE_FIXED_PAGES]);

    }
    
    printf("\n");
}

