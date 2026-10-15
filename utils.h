#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define THROW(err) do{ perror(err); exit(1); }while(0)
#define DA_INIT_SIZE 16
#define HM_INIT_SIZE 16
#define HM_LOAD_FAC 0.7
#define HM_NONE SIZE_MAX


// Dynamic array
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


// Hashmap
typedef struct {
    size_t len, cap;
    size_t *pos;
    uint64_t *hashes;
} _hm_hdr_t;

// Write key-value pair to hashmap
#define hm_put(hm, k, v) (hm = _hm_put(hm, k, sizeof(*hm)), \
    (hm)[hm_geti(hm, k)].val = v)
void *_hm_put(void *hm, const char *key, size_t esize);

// Get hashmap value index by key
size_t hm_geti(void *hm, const void *key);

// Get hashmap item count
size_t hm_len(void *hm);

// Free hashmap
void hm_free(void *hm);



#ifdef UTILS_IMPL
// Dynamic array
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

// Hashmap
#define _hm_hdr(hm) (((_hm_hdr_t*)hm) - 1)
static uint64_t _hm_hash(const char* str) {
    char *p = (char*)str;
    const char* end = str + strlen(str);

    // FNV-1a hash function
    uint64_t h = 14695981039346656037ull;
    while (p < end)
        h = (h ^ *p++) * 1099511628211ull;

    // David Stafford's Mix13 for MurmurHash3's 64-bit finalizer
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
    return h ^ (h >> 31);
}
static _hm_hdr_t *_hm_init(void **p_hm, size_t esize) {
    _hm_hdr_t *hdr = malloc(sizeof(*hdr) + esize * HM_INIT_SIZE);
    hdr->len = 0;
    hdr->cap = HM_INIT_SIZE;
    hdr->hashes = malloc(HM_INIT_SIZE * (sizeof(uint64_t) + sizeof(size_t)));
    hdr->pos = (size_t*)(hdr->hashes + HM_INIT_SIZE);

    for (size_t i = 0; i < HM_INIT_SIZE; i++)
        hdr->pos[i] = HM_NONE;
    *p_hm = hdr + 1;
    return hdr;
}
static size_t _hm_slot(const _hm_hdr_t *hdr, uint64_t hash) {
    size_t mask = hdr->cap - 1,
           pos  = (size_t)hash & mask,
           step = 0;
    while (hdr->pos[pos] != HM_NONE && hdr->hashes[pos] != hash)
        pos = (pos + ++step) & mask;
    return pos;
}
static void _hm_grow(void **p_hm, size_t esize) {
    _hm_hdr_t *hdr = _hm_hdr(*p_hm);
    size_t ocap = hdr->cap,
           ncap = ocap << 1;
    size_t    *opos = hdr->pos;
    uint64_t *ohash = hdr->hashes;

    hdr = realloc(hdr, sizeof(*hdr) + esize * (ncap + 1));
    hdr->hashes = malloc(ncap * (sizeof(uint64_t) + sizeof(size_t)));
    hdr->pos    = (size_t*)(hdr->hashes + ncap);
    hdr->cap    = ncap;
    *p_hm = (char*)(hdr+1) + esize;

    for (size_t i = 0; i < ncap; i++)
        hdr->pos[i] = HM_NONE;
    for (size_t i = 0; i < ocap; i++) {
        if (opos[i] == HM_NONE)
            continue;
        size_t pos = _hm_slot(hdr, ohash[i]);
        hdr->hashes[pos] = ohash[i];
        hdr->pos[pos] = opos[i];
    }
    free(ohash);
}
static size_t _hm_alloc(void **p_hm, const char *key, size_t esize) {
    _hm_hdr_t *hdr = *p_hm == NULL
                    ? _hm_init(p_hm, esize)
                    : _hm_hdr(*p_hm);
    if (hdr->len + 1 > hdr->cap * HM_LOAD_FAC) {
        _hm_grow(p_hm, esize);
        hdr = _hm_hdr(*p_hm);
    }

    uint64_t hash = _hm_hash(key);
    size_t pos = _hm_slot(hdr, hash);
    if (hdr->pos[pos] == HM_NONE) {
        hdr->hashes[pos] = hash;
        hdr->pos[pos] = hdr->len++;
        memset((char*)*p_hm + hdr->pos[pos] * esize, 0, esize);
    }
    return hdr->pos[pos];
}
size_t hm_geti(void *hm, const void *key) {
    _hm_hdr_t *hdr = _hm_hdr(hm);
    size_t pos = _hm_slot(hdr, _hm_hash(key));
    return hdr->pos[pos];
}
void *_hm_put(void *hm, const char *key, size_t esize) {
    size_t pos = _hm_alloc(&hm, key, esize);
    char **slot = (char**)((char*)hm + pos * esize);
    if (*slot == NULL) *slot = (char*)key;
    return hm;
}
size_t hm_len(void *hm) {
    _hm_hdr_t *hdr = _hm_hdr(hm);
    return hdr->len;
}
void hm_free(void *hm) {
    if (hm == NULL) return;
    _hm_hdr_t *hdr = _hm_hdr(hm);
    free(hdr->hashes);
    free(hdr);
}
#endif // UTILS_IMPL
#endif // UTILS_H
