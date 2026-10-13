#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define THROW(err) do{ perror(err); exit(1); }while(0)
#define DA_INIT_SIZE 16


typedef struct {
    void *data;
    size_t len, cap, esize;
} da_t;

// Initialize dynamic array
da_t *da_init(const size_t size);

// Free dynamic array (excl. elements)
void da_free(da_t *p);

// Allocate new element returning it's pointer
void *da_add(da_t *p);


#ifdef UTILS_IMPL
da_t *da_init(const size_t size) {
    da_t *p = malloc(sizeof(da_t));
    p->data = malloc(DA_INIT_SIZE * size);
    p->len = 0;
    p->cap = DA_INIT_SIZE;
    p->esize = size;
    return p;
}
void da_free(da_t *p) { free(p->data); free(p); }
void *da_add(da_t *p) {
    if (p->len == p->cap)
        p->data = realloc(p->data, (p->cap <<= 1) * p->esize);
    return (char*)p->data + (p->len++) * p->esize;
}
#endif // UTILS_IMPL
#endif // UTILS_H
