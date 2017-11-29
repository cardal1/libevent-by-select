//
// Created by james on 23/11/2017.
//

#include <stddef.h>
#include "evthread-internal.h"
#include "thread.h"

/* globals */
GLOBAL int evthread_lock_debugging_enabled_ = 0;
GLOBAL struct evthread_lock_callbacks evthread_lock_fns_ = {
        0, 0, NULL, NULL, NULL, NULL
};
GLOBAL unsigned long (*evthread_id_fn_)(void) = NULL;

GLOBAL struct evthread_condition_callbacks evthread_cond_fns_ = {
        0, NULL, NULL, NULL, NULL
};

/* Used for debugging */
static struct evthread_lock_callbacks original_lock_fns_ = {
        0, 0, NULL, NULL, NULL, NULL
};
static struct evthread_condition_callbacks original_cond_fns_ = {
        0, NULL, NULL, NULL, NULL
};

void
evthread_set_id_callback(unsigned long (*id_fn)(void))
{
 evthread_id_fn_ = id_fn;
}

struct evthread_lock_callbacks *evthread_get_lock_callbacks()
{
 return evthread_lock_debugging_enabled_
        ? &original_lock_fns_ : &evthread_lock_fns_;
}
struct evthread_condition_callbacks *evthread_get_condition_callbacks()
{
 return evthread_lock_debugging_enabled_
        ? &original_cond_fns_ : &evthread_cond_fns_;
}
void evthreadimpl_disable_lock_debugging_(void)
{
 evthread_lock_debugging_enabled_ = 0;
}



#define GLOBAL static
#define DEBUG_LOCK_SIG	0xdeb0b10c

GLOBAL unsigned long (*evthread_id_fn_)(void) = NULL;

struct debug_lock {
    unsigned signature;
    unsigned locktype;
    unsigned long held_by;
    /* XXXX if we ever use read-write locks, we will need a separate
     * lock to protect count. */
    int count;
    void *lock;
};

int
evthread_is_debug_lock_held_(void *lock_)
{
 struct debug_lock *lock = lock_;
 if (! lock->count)
  return 0;
 if (evthread_id_fn_) {
  unsigned long me = evthread_id_fn_();
  if (lock->held_by != me)
   return 0;
 }
 return 1;
}