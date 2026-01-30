#ifndef CONTAINER_VERSION_H
#define CONTAINER_VERSION_H

#include<time.h>
#include "settings.h"
#include "slabInt.h"
#include "slab.h"
#include "cont.h"

#define VERSION_DIR_MEMSIZE    (5 * sizeof(uint64_t))           /* version_dir_index */ 
//#define VERSION_ENTRIES    ((PAGE_SIZE - VERSION_DIR_MEMSIZE) / \
//                                ( sizeof(struct version_entry)))

#define VERSION_ENTRIES    80                      

struct version_entry{

    unsigned int version_id;
    time_t timestamp;
    
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } current_version_slab;

    //Required:???
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } snapshot_version_slab;
};

struct version_dir{
    struct version_entry version_entries[VERSION_ENTRIES];
    struct {
        struct version_dir *maddr;
        size_t laddr;
    } next_current_version_dir;
    struct {
        struct version_dir *maddr;
        size_t laddr;
    } next_snapshot_version_dir;
};

/* allocate a version_dir */
struct version_dir* version_dir_init(unsigned int cid, size_t *laddr);
void print_version_entries(struct container* cont, struct version_dir* version_dir);
struct version_entry* get_version_entry_by_id(struct container* cont,struct version_dir* vd, unsigned int vid);

#endif