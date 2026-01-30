#ifndef POINTER_LOCATION
#define POINTER_LOCATION

#include<stddef.h>

#include "settings.h"
#include "slabInt.h"
#include "macros.h"
#include "cont.h"
#include "stats.h"
#include "atomics.h"




#define POINTER_ENTRIES_SIZE    ((PAGE_SIZE) / \
                                ( sizeof(void*) + sizeof(uint16_t) + sizeof(unsigned int)))
#define POINTER_LOCATIONS_PAGES_SIZE    ((PAGE_SIZE) / \
                                ( 4 * sizeof(void*)))
struct pointer_locations_page{
     struct {
        struct pointer_page *maddr;
        size_t laddr;
    } pointer_pages[POINTER_LOCATIONS_PAGES_SIZE];

    struct {
        struct pointer_page *maddr;
        size_t laddr;
    } pointer_pages_ss[POINTER_LOCATIONS_PAGES_SIZE];
};



struct pointer_page{

    size_t pointer_laddr[POINTER_ENTRIES_SIZE];
    unsigned int  pval_slab_entry[POINTER_ENTRIES_SIZE];
    uint16_t pval_offset[POINTER_ENTRIES_SIZE];
};

struct pointer_locations_page* pointer_locations_page_snapshot(unsigned int cid, struct 
pointer_locations_page *plp, size_t* laddr);

struct pointer_page* pointer_page_snapshot(unsigned int cid, struct 
pointer_page *pp, size_t* laddr);

int insert_pointer_location(int cid, size_t pointer_location, unsigned int pval_slab_entry, uint16_t pval_offset);
int delete_pointer_location(int cid, size_t pointer_location);

void test_insert(int cid, size_t laddr, unsigned int pval_slab_entry, uint16_t pval_offset);
void test_delete(int cid, size_t addr);
void print_all_pointer_locations(int cid);
int insert_pointer_locations_in_ht_pointerat(int cid);



#endif