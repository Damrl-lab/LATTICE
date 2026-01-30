#include <string.h>
#include <atomics.h>
#include <settings.h>
#include <macros.h>
#include <libpmem.h>


// Flusing operation using libpmem library
void flush_memsegment(const void *src, size_t n, int fence)
{
   pmem_flush(src, ROUND_UPCL(n));
}

/**
 * @dest is assumed to be in persistent memory
 */

// void *pmemcpy(void *dest, const void *src, size_t n)
// {
//     void *ret = memcpy(dest, src, n);
//     flush_memsegment(dest, n, 0);
//     return ret;
// }


 void *pmemcpy(void *dest, const void *src, size_t n)
 {
     
     void* ret = pmem_memcpy(dest,src,n,PMEM_F_MEM_NODRAIN);

    return ret;
 }