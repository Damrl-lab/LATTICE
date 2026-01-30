#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include <libpmem.h>
#include <omp.h>



#include "slabInt.h"
#include "cont.h"
#include "atomics.h"
#include "page_alloc.h"
#include "container_version.h"
#include "pointer_locations.h"


double what_is_the_time_now()
{
    struct timeval time;
    if (gettimeofday(&time, NULL)) {
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

int total_shadow_copy_time = 0;

struct container* ptr_container;

struct address_map{
    size_t original;
    size_t copy;
    struct address_map* next;
};
struct address_map *head, *current;

static void print_address_map()
{
    printf("printing address map:\n\n");
    struct address_map* am;
    am = head;
    while(am)
    {
        printf("original: %d copy: %d\n",am->original,am->copy);
        am = am->next;
    }
}

static void slab_bucket_cpoint(unsigned int cid,struct slab_dir* sd_version, struct slab_bucket *sb, 
struct slab_bucket  *sb_version, int type)
{
    
    int i = 0;
    struct version_entry ve;
    #pragma omp parallel 
    {
       #pragma omp for 
        for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
            
            struct slab_entry *se;
            struct slab_entry *se_v;
            int npages = 1;
            se = &sb->sb_entries[i];
            if (!SLAB_ENTRY_IS_INIT(se))
                continue;
            se_v = &sb_version->sb_entries[i];
            npages = ROUNDPG(se->se_size)/(PAGE_SIZE);
            if (se->modified == '1') {

                #pragma omp critical 
                {
                    if(npages == 1)
                        se_v->se_data.current.maddr = page_allocator_getpage(cid, &se_v->se_data.current.laddr, PA_PROT_RNW);
                    else
                        se_v->se_data.current.maddr = page_allocator_get_contiguous_pages(cid, npages, &se_v->se_data.current.laddr, PA_PROT_RNW);
                    if (se_v->se_data.current.laddr == NULL)
                        handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");

                }
                pmemcpy(se_v->se_data.current.maddr, se->se_data.current.maddr, PAGE_SIZE*npages);

                struct version_entry ve2;
                  
                atomic_set(&se->se_data.snapshot.laddr, se_v->se_data.current.laddr);
                atomic_set(&se_v->se_data.snapshot.laddr, se_v->se_data.current.laddr);
                //todo: ashikee the following two assigns should be made atomic
                se->modified = '0';
                se_v->modified = '0';
                #pragma omp critical 
                 {
                    VECTOR_APPEND(&sd_version->sd_vector,se_v);
                 }
                se->se_data.snapshot.maddr = se_v->se_data.current.maddr;
                se_v->se_data.snapshot.maddr = se_v->se_data.current.maddr;    
            }
            else
            {
                    atomic_set(&se_v->se_data.current.laddr, se->se_data.snapshot.laddr);
                    atomic_set(&se_v->se_data.current.maddr, se->se_data.snapshot.maddr);
                    atomic_set(&se_v->se_data.snapshot.maddr, se->se_data.snapshot.maddr);
                    atomic_set(&se_v->se_data.snapshot.laddr, se->se_data.snapshot.laddr);
            }
             if (type == CPOINT_REGULAR)
            { 
                page_allocator_mprotect(cid, se_v->se_data.current.maddr, PAGE_SIZE*npages, PROT_READ);
            }
            #pragma omp critical 
            {
                if(!current)
                {
                    head = malloc(sizeof(struct address_map));
                    head->original = se->se_data.current.laddr;
                    head->copy = se_v->se_data.current.laddr;
                    current = head;
                    head->next = NULL;

                }
                else
                {
                    current->next = malloc(sizeof(struct address_map));
                    current = current->next;
                    current->original = se->se_data.current.laddr;
                    current->copy = se_v->se_data.current.laddr;
                    current->next = NULL;
                }

            }

        }

    }
    //todo: ashikee the following two assigns should be made atomic
    sb->modified = '0';
    sb_version->modified = '0';
}

static void slab_inner_cpoint(unsigned int cid,struct slab_dir* sd_version, struct slab_inner *si,
struct slab_inner *si_version, int type)
{
    int i;
    struct slab_bucket *sb;

    for (i = 0; i < si->si_index; i++) {
        sb = si->si_current[i].maddr;
        if(sb->modified == '1'){
            //todo ashikee : optimize sb_has_snapshot ,set true currently
            size_t sb_version_laddr;
            struct slab_bucket *sb_version = page_allocator_getpage(cid, &sb_version_laddr, PA_PROT_WRITE);
            if (sb_version_laddr == NULL)
                handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
            pmemcpy(sb_version, sb, PAGE_SIZE);
            STATS_INC_COWDATA();
            slab_bucket_cpoint(cid,sd_version, sb,sb_version, type);
            atomic_set(&si->si_snapshot[i].laddr, si->si_current[i].laddr);
            atomic_set(&si_version->si_current[i].laddr, sb_version_laddr);
            atomic_set(&si_version->si_snapshot[i].laddr, sb_version_laddr);
            si->si_snapshot[i].maddr = si->si_current[i].maddr;
            si_version->si_current[i].maddr = sb_version;
            si_version->si_snapshot[i].maddr = sb_version;        
        }
      
    }
    //todo: ashikee the following two assigns should be made atomic
    si->modified = '0';
    si_version->modified = '0';
}

static void slab_outer_cpoint(unsigned int cid,struct slab_dir* sd_version, struct slab_outer *so, struct 
slab_outer *so_version, int type)
{
    int i;

    for (i = 0; i < so->so_index; i++) {
        struct slab_inner* si = so->so_current[i].maddr;
        if(si->modified == '1')
        {

            size_t si_version_laddr;
            struct slab_inner *si_version = page_allocator_getpage(cid, &si_version_laddr, PA_PROT_WRITE);
            if (si_version_laddr == NULL)
                handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
            pmemcpy(si_version, si, PAGE_SIZE);
            STATS_INC_COWDATA();
            slab_inner_cpoint(cid, sd_version,so->so_current[i].maddr,si_version, type);

             if (so->so_current[i].maddr != so->so_snapshot[i].maddr) 
             {
                atomic_set(&so->so_snapshot[i].laddr, so->so_current[i].laddr);
                page_allocator_freepages(cid, so->so_snapshot[i].maddr);
             }

            atomic_set(&so_version->so_snapshot[i].laddr, si_version_laddr);
            atomic_set(&so_version->so_current[i].laddr, si_version_laddr);
            so->so_snapshot[i].maddr = so->so_current[i].maddr;
            so_version->so_snapshot[i].maddr = si_version;
            so_version->so_current[i].maddr = si_version;

             
        }

    }
    //todo: ashikee the following two assigns should be made atomic
    so->modified = '0';
    so_version->modified = '0';
}

static void slab_dir_cpoint(unsigned int cid, struct slab_dir *sd, 
struct slab_dir *sd_version, int type)
{
    int i;
    for (i = 0; i < sd->sd_index; i++) {
        struct slab_outer* so = sd->sd_current[i].maddr;
        if(so->modified == '1')
        {
            size_t so_version_laddr;
            struct slab_outer *so_version = page_allocator_getpage(cid, &so_version_laddr, PA_PROT_WRITE);
            if (so_version_laddr == NULL)
                handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
            pmemcpy(so_version, so, PAGE_SIZE);
            STATS_INC_COWDATA();

            slab_outer_cpoint(cid, sd_version, sd->sd_current[i].maddr, so_version,type);
             if (sd->sd_current[i].maddr != sd->sd_snapshot[i].maddr) {  
                atomic_set(&sd->sd_snapshot[i].laddr, sd->sd_current[i].laddr); 
                page_allocator_freepages(cid, sd->sd_snapshot[i].maddr);
             }
            atomic_set(&sd_version->sd_snapshot[i].laddr, so_version_laddr);
            atomic_set(&sd_version->sd_current[i].laddr, so_version_laddr);
            sd->sd_snapshot[i].maddr = sd->sd_current[i].maddr;
            sd_version->sd_current[i].maddr = so_version;
            sd_version->sd_snapshot[i].maddr = so_version;
        }
    }     
    pointer_pages_cpoint(cid,sd,sd_version, type);
    //todo: ashikee the following two assigns should be made atomic
    sd->modified = '0';
    sd_version->modified = '0';
   

}
 void pointer_pages_cpoint(unsigned int cid, struct slab_dir *sd, 
struct slab_dir *sd_version, int type)
{
    struct pointer_locations_page* plp = sd->pointer_locations_page.maddr;
    sd_version->pointer_locations_page.maddr = page_allocator_getpage(cid, &sd_version->pointer_locations_page.laddr, PA_PROT_WRITE);
    if (sd_version->pointer_locations_page.laddr == NULL)
    handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
    STATS_INC_COWDATA();
    struct pointer_locations_page* plp_version = sd_version->pointer_locations_page.maddr;

    for(int i = 0; i < POINTER_LOCATIONS_PAGES_SIZE ; i++)
    {
           if(plp->pointer_pages[i].maddr)
        {
            plp_version->pointer_pages[i].maddr = page_allocator_getpage(cid,
             &plp_version->pointer_pages[i].laddr, PA_PROT_WRITE);
            
            if(plp_version->pointer_pages[i].laddr == NULL)
                handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
           
            pmemcpy(plp_version->pointer_pages[i].maddr, plp->pointer_pages[i].maddr, PAGE_SIZE);
            STATS_INC_COWDATA();
            struct pointer_page *ppage =  plp->pointer_pages[i].maddr;
            struct pointer_page *ppage_version = plp_version->pointer_pages[i].maddr;

            for(int j = 0; j < POINTER_ENTRIES_SIZE;j++)
            {
                size_t page_loc = (ppage->pointer_laddr[j]/PAGE_SIZE)*PAGE_SIZE;
                size_t off_set = ppage->pointer_laddr[j]%PAGE_SIZE;
                struct address_map* am = head;
                while(am)
                {
                    if(page_loc == am->original)
                    {
                        page_loc = am->copy;
                        ppage_version->pointer_laddr[j] = page_loc + off_set;
                        break;
                    }
                    am = am->next;
                }
               
            }
            atomic_set(&plp_version->pointer_pages_ss[i].laddr, plp_version->pointer_pages[i].laddr);
            atomic_set(&plp_version->pointer_pages_ss[i].maddr, plp_version->pointer_pages[i].maddr);


            if(plp->pointer_pages[i].maddr != plp->pointer_pages_ss[i].maddr)
            {
                 atomic_set(&plp->pointer_pages_ss[i].laddr, plp->pointer_pages[i].laddr);
                 if(plp->pointer_pages_ss[i].maddr)
                     page_allocator_freepages(cid, plp->pointer_pages_ss[i].maddr);
                 plp->pointer_pages_ss[i].maddr = plp->pointer_pages[i].maddr;
            }
        }
        else break;
    }
    
    atomic_set(&sd_version->snapshot_pointer_locations_page.laddr, sd_version->pointer_locations_page.laddr);
    sd_version->snapshot_pointer_locations_page.maddr = sd_version->pointer_locations_page.maddr;
    if(sd->pointer_locations_page.maddr != sd->snapshot_pointer_locations_page.maddr)
    {
        atomic_set(&sd->snapshot_pointer_locations_page.laddr, sd->pointer_locations_page.laddr);
        if(sd->snapshot_pointer_locations_page.maddr)
            page_allocator_freepages(cid, sd->snapshot_pointer_locations_page.maddr);
        sd->snapshot_pointer_locations_page.maddr = sd->pointer_locations_page.maddr;
    }
}


struct async_parameter{

    unsigned int cid;
    int type;
};

void async_cpy(void *p)
{
    double st_time = what_is_the_time_now();

    struct async_parameter* ap =  (struct async_parameter*) p;
    int type = ap->type;
    int cid = ap->cid;

    struct slab_dir *sd;
    struct container *cont = get_container(cid);
    sd = cont->current_slab.maddr;
    sd->container_begin_addr_in_last_commit = cont;
    struct slab_dir *sd_version;
    #pragma omp parallel
    {
        #pragma omp for 
        for (int i = 0; i < VECTOR_SIZE(&sd->sd_vector); ++i) {
            struct slab_entry *se = NULL;
            se = VECTOR_AT(&sd->sd_vector, i);
            int sz = ROUNDPG(se->se_size);
            flush_memsegment(se->se_data.current.maddr,  sz,0);
            page_allocator_mprotect(cid, se->se_data.current.maddr, sz, PROT_READ);
        
        }  

    }
    
    if(sd->modified == '1')
    {
        size_t sd_version_laddr;
        sd_version = page_allocator_getpage(cid, &sd_version_laddr, PA_PROT_WRITE);
        if (sd_version_laddr == NULL)
            handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
        pmemcpy(sd_version, sd, PAGE_SIZE);
        STATS_INC_COWDATA();

         //VECTOR_FREE(&sd_version->sd_vector);
        VECTOR_INIT(&sd_version->sd_vector);
        sd_version->container_begin_addr_in_last_commit = get_container(cid);
        slab_dir_cpoint(cid, sd, sd_version, type);   
       
        struct container* current_container = get_container(cid);
        struct version_dir* vd =   current_container->current_version_dir.maddr;

        while(vd->next_current_version_dir.maddr != NULL)
            vd = vd->next_current_version_dir.maddr;
        if(current_container->version_dir_index%VERSION_ENTRIES == 0 &&
           vd->version_entries[VERSION_ENTRIES-1].timestamp != 0)
        {
            vd->next_current_version_dir.maddr = 
            page_allocator_getpage(cid,  &vd->next_current_version_dir.laddr, PA_PROT_WRITE);
            vd->next_snapshot_version_dir.maddr =  vd->next_current_version_dir.maddr;
            vd->next_snapshot_version_dir.laddr =  vd->next_current_version_dir.laddr;
            vd = vd->next_current_version_dir.maddr;
            vd->next_current_version_dir.maddr = NULL;
            vd->next_current_version_dir.laddr = 0;
            vd->next_snapshot_version_dir.maddr = NULL;
            vd->next_snapshot_version_dir.laddr = 0;
            vd->version_entries[VERSION_ENTRIES-1].timestamp = 0;
        }
        vd->version_entries[current_container->version_dir_index%VERSION_ENTRIES].current_version_slab.maddr
         = sd_version;
        vd->version_entries[current_container->version_dir_index%VERSION_ENTRIES].current_version_slab.laddr 
        = ptoi(sd_version)- ptoi(get_container(cid));

        vd->version_entries[current_container->version_dir_index%VERSION_ENTRIES].snapshot_version_slab.maddr
         = sd_version;
        vd->version_entries[current_container->version_dir_index%VERSION_ENTRIES].snapshot_version_slab.laddr
         = ptoi(sd_version)- ptoi(get_container(cid));
        vd->version_entries[current_container->version_dir_index%VERSION_ENTRIES].version_id
         = cont->version_dir_index * 1000;
        vd->version_entries[current_container->version_dir_index%VERSION_ENTRIES].timestamp 
        = time(NULL);
        current_container->version_dir_index = current_container->version_dir_index + 1;

        struct container *cont = get_container(cid);
        cont->head_slab.laddr = cont->current_slab.laddr;
        cont->head_slab.maddr = cont->current_slab.maddr;
        cont->head_snapshot_slab.laddr = cont->snapshot_slab.laddr;
        cont->head_snapshot_slab.maddr = cont->snapshot_slab.maddr;
    }

    struct slab_entry *se = NULL;    
    VECTOR_FREE(&sd->sd_vector);
    if(sd_version)
    {
         for (int i = 0; i < VECTOR_SIZE(&sd_version->sd_vector); ++i) {
            se = VECTOR_AT(&sd_version->sd_vector, i);
            flush_memsegment(se->se_data.current.maddr, PAGE_SIZE, 0);
        }
        VECTOR_FREE(&sd_version->sd_vector);

    }

    struct address_map* node;
    while(head != NULL)
    {
        node = head;
        head = head->next;
        free(node);
    }
    head = NULL;
    current = NULL;

    cont = get_container(cid);
    if (cont->current_slab.maddr != cont->snapshot_slab.maddr) {
        atomic_set(&cont->snapshot_slab.laddr, cont->current_slab.laddr);
        page_allocator_freepages(cont->id, cont->snapshot_slab.maddr);
        cont->snapshot_slab.maddr = cont->current_slab.maddr;
    }

    cont->pg_allocator->pa_ops->
    copy_mapper_to_container(cont->pg_allocator->pa_handler,cont);
    page_allocator_mprotect(cid, cont->snapshot_fixed_pages.maddr,
     PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    pmemcpy(cont->snapshot_fixed_pages.maddr,cont->fixed_pages.maddr,PAGE_SIZE);
    //todo ashikee change it to atomic flag
    flush_memsegment(cont->snapshot_fixed_pages.maddr, PAGE_SIZE, 0);
    flush_memsegment(cont->fixed_pages.maddr, PAGE_SIZE, 0);
    //TODO: make sure that all changes to PM up to this point are durable
    atomic_clear_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS);
    pmem_drain();
   
    double end_time = what_is_the_time_now() - st_time;
    total_shadow_copy_time += end_time;
 //  printf("async copy time:%f\n",end_time);
}


void slab_cpoint(unsigned int cid, int type,int is_async)
{
    struct slab_dir *sd;
    struct container *cont;
    cont = get_container(cid);
    ptr_container = get_container(cid);
    sd = cont->current_slab.maddr;
    sd->container_begin_addr_in_last_commit = cont;
    struct slab_dir *sd_version;
    
    
   
    container_set_checkpoint_phase(CP_SHADOW_COPY_LIBVPM);  
    struct async_parameter* ap = malloc(sizeof(struct async_parameter));
    ap->cid = cid;
    ap->type = type;
    if( is_async)
    {
        pmem_drain();
        pthread_t async_cpy_thread;
        pthread_create(&async_cpy_thread, NULL,async_cpy, (void *)ap );
        return;
    }
    async_cpy(ap);   
}
