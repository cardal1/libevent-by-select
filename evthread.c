//
// Created by james on 23/11/2017.
//

#include <stddef.h>

int evthread_lock_debugging_enabled_ = 0;

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