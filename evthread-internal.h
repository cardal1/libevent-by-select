//
// Created by james on 23/11/2017.
//

#ifndef LIBEVENT_TEST_EVTHREAD_INTERNAL_H
#define LIBEVENT_TEST_EVTHREAD_INTERNAL_H

#define GLOBAL static

GLOBAL extern int evthread_lock_debugging_enabled_;

extern unsigned long (*evthread_id_fn_)(void);

/** Return true iff we're in the thread that is currently (or most recently)
 * running a given event_base's loop. Requires lock. */
#define EVBASE_IN_THREAD(base)				 \
	(evthread_id_fn_ == NULL ||			 \
	(base)->th_owner_id == evthread_id_fn_())


/** If lock debugging is enabled, and lock is non-null, assert that 'lock' is
 * locked and held by us. */
#define EVLOCK_ASSERT_LOCKED(lock)					\
	do {								\
		if ((lock) && evthread_lock_debugging_enabled_) {	\
			EVUTIL_ASSERT(evthread_is_debug_lock_held_(lock)); \
		}							\
	} while (0)

#endif //LIBEVENT_TEST_EVTHREAD_INTERNAL_H


int evthread_is_debug_lock_held_(void *lock);
void *evthread_debug_get_real_lock_(void *lock);