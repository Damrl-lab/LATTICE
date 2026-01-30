#include "container_version.h"
#include "cont.h"
#include "page_alloc.h"
#include "assert.h"
#include "atomics.h"

struct version_dir* version_dir_init(unsigned int cid, size_t *laddr)
{
    struct version_dir *vd = NULL;

    vd = (struct version_dir*) page_allocator_getpage(cid, laddr, PA_PROT_WRITE);
     if (!vd)
        handle_error("failed to allocate memory for version dir\n");
    vd->next_current_version_dir.maddr = NULL;
    vd->next_current_version_dir.laddr = 0;
    vd->next_snapshot_version_dir.maddr = NULL;
    vd->next_snapshot_version_dir.laddr = NULL;
    vd->version_entries[VERSION_ENTRIES-1].timestamp = 0;
    return vd;
}


struct version_entry* get_version_entry_by_id(struct container* cont,struct version_dir* vd, unsigned int vid)
{
    int i = 0;
    struct version_entry* ve = NULL;
    while(i < cont->version_dir_index)
    {
        if(i% VERSION_ENTRIES == 0 &&
           vd->version_entries[VERSION_ENTRIES-1].timestamp != 0)
            vd = vd->next_current_version_dir.maddr;
        if(vd->version_entries[i% VERSION_ENTRIES ].version_id == vid)
        {
            return &vd->version_entries[i% VERSION_ENTRIES ];
        }
        i++;
    }

    return ve;

}

//debug

void print_version_entries(struct container* cont, struct version_dir* version_dir)
{
    int i = 0;
    printf("Current version directory index:%d\n ",cont->version_dir_index);
    while(i < cont->version_dir_index)
    {
        if(i% VERSION_ENTRIES == 0 &&
           version_dir->version_entries[VERSION_ENTRIES-1].timestamp != 0)
            version_dir = version_dir->next_current_version_dir.maddr;
        printf("Version: %d\n",i+1);
        printf("    version_id:%d \n",version_dir->version_entries[i% VERSION_ENTRIES ].version_id);
        printf("    current_version_slab maddr:%p \n",version_dir->version_entries[i% VERSION_ENTRIES ].
        current_version_slab.maddr);
        printf("    current_version_slab laddr:%p \n",version_dir->version_entries[i% VERSION_ENTRIES ].
        current_version_slab.laddr);
        printf("    snapshot_version_slab maddr:%p \n",version_dir->version_entries[i% VERSION_ENTRIES ].
        snapshot_version_slab.maddr);
        printf("    snapshot_version_slab laddr:%p \n",version_dir->version_entries[i% VERSION_ENTRIES ].
        snapshot_version_slab.maddr);
        i++;
    }
}

