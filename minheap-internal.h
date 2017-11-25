//
// Created by james on 25/11/2017.
//

#ifndef LIBEVENT_BY_SELECT_MINHEAP_INTERNAL_H
#define LIBEVENT_BY_SELECT_MINHEAP_INTERNAL_H

#include "mm-internal.h"

typedef struct min_heap
{
    struct event** p;
    unsigned n, a;
} min_heap_t;

static inline unsigned	     min_heap_size_(min_heap_t* s);

unsigned min_heap_size_(min_heap_t* s) { return s->n; }

int min_heap_reserve_(min_heap_t* s, unsigned n)
{
    if (s->a < n)
    {
        struct event** p;
        unsigned a = s->a ? s->a * 2 : 8;
        if (a < n)
            a = n;
        if (!(p = (struct event**)mm_realloc(s->p, a * sizeof *p)))
            return -1;
        s->p = p;
        s->a = a;
    }
    return 0;
}

#endif //LIBEVENT_BY_SELECT_MINHEAP_INTERNAL_H
