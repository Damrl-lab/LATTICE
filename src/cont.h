#ifndef CONT_H
#define CONT_H

//#include <assert.h>

//#include "settings.h"

#include "page_alloc.h"
#include "fixptr.h"
#include "closure.h"
#include "fixmapper.h"

#define CPOINT_IN_PROGRESS_BIT 0
#define CFLAG_CPOINT_IN_PROGRESS    (1 << CPOINT_IN_PROGRESS_BIT)
#define MAX_UNSIGNED_INT 4294967295
#define NO_CP_ONGOING_LIBVPM  0
#define CP_COPY_LIBVPM 1
#define CP_SHADOW_COPY_LIBVPM 2

struct container{
    unsigned int id;
    struct page_allocator *pg_allocator;
    int is_not_head;
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } current_slab;
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } snapshot_slab;

     struct {
        struct slab_dir *maddr;
        size_t laddr;
    } head_slab;
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } head_snapshot_slab;

    struct {
        struct version_dir *maddr;
        size_t laddr;
    } current_version_dir;
    struct {
        struct version_dir *maddr;
        size_t laddr;
    } snapshot_version_dir;
     struct {
        struct free_pages_segment *maddr;
        size_t laddr;
    } fixed_pages;
    struct {
        struct free_pages_segment *maddr;
        size_t laddr;
    } snapshot_fixed_pages;

    unsigned char flags;
    uint32_t total_number_of_free_pages;
    uint32_t version_dir_index;


    //STAILQ_HEAD(ptrat_list, ptrat) ptrat_head; ///< keep all ptrs from pointerat to be added at cpoint
};

struct container *container_init();
struct container *container_initialization(int cid);
struct container *container_init_cid(int cid);


void *container_palloc(unsigned int cid, unsigned int size);
int container_cpoint(unsigned int cid);
int container_checkpoint(unsigned int cid, int is_async);
int container_checkpoint_with_phase(unsigned int cid, int is_async, int* checkpoint_phase);

struct container* container_restore(unsigned int cid);
struct container *container_restore_for_version(unsigned int cid, unsigned int version_id);

size_t container_setroot(unsigned int cid, void *maddr);
void *container_getroot(unsigned int cid);

void container_set_checkpoint_phase(int phase);

void container_pprint();

void container_test_pointer_insert(unsigned int cid, void* addr, unsigned int  se_id, uint16_t  offset);
void container_test_pointer_delete(unsigned int cid, void* addr);
struct version_dir *get_version_dir(unsigned int cid);
int is_restoring();

#endif /* end of include guard: CONT_H */
