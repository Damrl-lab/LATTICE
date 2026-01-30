#ifndef HTABLE_H
#define HTABLE_H

#include <stdint.h>

struct htable;

struct htable* htable_init();
void htable_free(struct htable*);

void *htable_lookup(struct htable*, void *key);
int htable_find(struct htable *ht, void *key);
int htable_insert(struct htable* ht, void *key, void *val);
int htable_insert_2(struct htable *ht, void *key, void *val, int v2);

void *htable_remove(struct htable*, void *key);

void htable_foreach(struct htable*, void (*fun)(void *key, void *val, void *param), void *param);
void htable_foreach_2(struct htable *ht,
                    void (*fun)(void *key, void *val, void *param, int val2),
                    void *param);
void htable_filter(struct htable *ht,int (*fun)(void *key, void *val, int val2, void *param),void *param);
#endif /* end of include guard: HTABLE_H */
