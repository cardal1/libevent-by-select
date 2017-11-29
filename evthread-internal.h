//
// Created by james on 23/11/2017.
//

#ifndef LIBEVENT_TEST_EVTHREAD_INTERNAL_H
#define LIBEVENT_TEST_EVTHREAD_INTERNAL_H

#define GLOBAL static

extern struct evthread_lock_callbacks evthread_lock_fns_;
extern struct evthread_condition_callbacks evthread_cond_fns_;
extern unsigned long (*evthread_id_fn_)(void);
extern int evthread_lock_debugging_enabled_;

/** Return the ID of the current thread, or 1 if threading isn't enabled. */
#define EVTHREAD_GET_ID() \
	(evthread_id_fn_ ? evthread_id_fn_() : 1)

/** Return true iff we're in the thread that is currently (or most recently)
 * running a given event_base's loop. Requires lock. */
#define EVBASE_IN_THREAD(base)				 \
	(evthread_id_fn_ == NULL ||			 \
	(base)->th_owner_id == evthread_id_fn_())

/** Return true iff we need to notify the base's main thread about changes to
 * its state, because it's currently running the main loop in another
 * thread. Requires lock. */
#define EVBASE_NEED_NOTIFY(base)			 \
	(evthread_id_fn_ != NULL &&			 \
	    (base)->running_loop &&			 \
	    (base)->th_owner_id != evthread_id_fn_())

/** Allocate a new lock, and store it in lockvar, a void*.  Sets lockvar to
    NULL if locking is not enabled. */
#define EVTHREAD_ALLOC_LOCK(lockvar, locktype)		\
	((lockvar) = evthread_lock_fns_.alloc ?		\
	    evthread_lock_fns_.alloc(locktype) : NULL)

/** Free a given lock, if it is present and locking is enabled. */
#define EVTHREAD_FREE_LOCK(lockvar, locktype)				\
	do {								\
		void *lock_tmp_ = (lockvar);				\
		if (lock_tmp_ && evthread_lock_fns_.free)		\
			evthread_lock_fns_.free(lock_tmp_, (locktype)); \
	} while (0)

/** Acquire a lock. */
#define EVLOCK_LOCK(lockvar,mode)					\
	do {								\
		if (lockvar)						\
			evthread_lock_fns_.lock(mode, lockvar);		\
	} while (0)

/** Release a lock */
#define EVLOCK_UNLOCK(lockvar,mode)					\
	do {								\
		if (lockvar)						\
			evthread_lock_fns_.unlock(mode, lockvar);	\
	} while (0)

/** Helper: put lockvar1 and lockvar2 into pointerwise ascending order. */
#define EVLOCK_SORTLOCKS_(lockvar1, lockvar2)				\
	do {								\
		if (lockvar1 && lockvar2 && lockvar1 > lockvar2) {	\
			void *tmp = lockvar1;				\
			lockvar1 = lockvar2;				\
			lockvar2 = tmp;					\
		}							\
	} while (0)

/** Lock an event_base, if it is set up for locking.  Acquires the lock
    in the base structure whose field is named 'lockvar'. */
#define EVBASE_ACQUIRE_LOCK(base, lockvar) do {				\
		EVLOCK_LOCK((base)->lockvar, 0);			\
	} while (0)

/** Unlock an event_base, if it is set up for locking. */
#define EVBASE_RELEASE_LOCK(base, lockvar) do {				\
		EVLOCK_UNLOCK((base)->lockvar, 0);			\
	} while (0)

/** If lock debugging is enabled, and lock is non-null, assert that 'lock' is
 * locked and held by us. */
#define EVLOCK_ASSERT_LOCKED(lock)					\
	do {								\
		if ((lock) && evthread_lock_debugging_enabled_) {	\
			EVUTIL_ASSERT(evthread_is_debug_lock_held_(lock)); \
		}							\
	} while (0)

/** Try to grab the lock for 'lockvar' without blocking, and return 1 if we
 * manage to get it. */
static inline int EVLOCK_TRY_LOCK_(void *lock);
static inline int
EVLOCK_TRY_LOCK_(void *lock)
{
    if (lock && evthread_lock_fns_.lock) {
        int r = evthread_lock_fns_.lock(EVTHREAD_TRY, lock);
        return !r;
    } else {
        /* Locking is disabled either globally or for this thing;
         * of course we count as having the lock. */
        return 1;
    }
}

/** Allocate a new condition variable and store it in the void *, condvar */
#define EVTHREAD_ALLOC_COND(condvar)					\
	do {								\
		(condvar) = evthread_cond_fns_.alloc_condition ?	\
		    evthread_cond_fns_.alloc_condition(0) : NULL;	\
	} while (0)
/** Deallocate and free a condition variable in condvar */
#define EVTHREAD_FREE_COND(cond)					\
	do {								\
		if (cond)						\
			evthread_cond_fns_.free_condition((cond));	\
	} while (0)
/** Signal one thread waiting on cond */
#define EVTHREAD_COND_SIGNAL(cond)					\
	( (cond) ? evthread_cond_fns_.signal_condition((cond), 0) : 0 )
/** Signal all threads waiting on cond */
#define EVTHREAD_COND_BROADCAST(cond)					\
	( (cond) ? evthread_cond_fns_.signal_condition((cond), 1) : 0 )
/** Wait until the condition 'cond' is signalled.  Must be called while
 * holding 'lock'.  The lock will be released until the condition is
 * signalled, at which point it will be acquired again.  Returns 0 for
 * success, -1 for failure. */
#define EVTHREAD_COND_WAIT(cond, lock)					\
	( (cond) ? evthread_cond_fns_.wait_condition((cond), (lock), NULL) : 0 )
/** As EVTHREAD_COND_WAIT, but gives up after 'tv' has elapsed.  Returns 1
 * on timeout. */
#define EVTHREAD_COND_WAIT_TIMED(cond, lock, tv)			\
	( (cond) ? evthread_cond_fns_.wait_condition((cond), (lock), (tv)) : 0 )

/** True iff locking functions have been configured. */
#define EVTHREAD_LOCKING_ENABLED()		\
	(evthread_lock_fns_.lock != NULL)

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

/* This code is shared between both lock impls */
#if ! defined(EVENT__DISABLE_THREAD_SUPPORT)
/** Helper: put lockvar1 and lockvar2 into pointerwise ascending order. */
#define EVLOCK_SORTLOCKS_(lockvar1, lockvar2)				\
	do {								\
		if (lockvar1 && lockvar2 && lockvar1 > lockvar2) {	\
			void *tmp = lockvar1;				\
			lockvar1 = lockvar2;				\
			lockvar2 = tmp;					\
		}							\
	} while (0)

/** Acquire both lock1 and lock2.  Always allocates locks in the same order,
 * so that two threads locking two locks with LOCK2 will not deadlock. */
#define EVLOCK_LOCK2(lock1,lock2,mode1,mode2)				\
	do {								\
		void *lock1_tmplock_ = (lock1);				\
		void *lock2_tmplock_ = (lock2);				\
		EVLOCK_SORTLOCKS_(lock1_tmplock_,lock2_tmplock_);	\
		EVLOCK_LOCK(lock1_tmplock_,mode1);			\
		if (lock2_tmplock_ != lock1_tmplock_)			\
			EVLOCK_LOCK(lock2_tmplock_,mode2);		\
	} while (0)
/** Release both lock1 and lock2.  */
#define EVLOCK_UNLOCK2(lock1,lock2,mode1,mode2)				\
	do {								\
		void *lock1_tmplock_ = (lock1);				\
		void *lock2_tmplock_ = (lock2);				\
		EVLOCK_SORTLOCKS_(lock1_tmplock_,lock2_tmplock_);	\
		if (lock2_tmplock_ != lock1_tmplock_)			\
			EVLOCK_UNLOCK(lock2_tmplock_,mode2);		\
		EVLOCK_UNLOCK(lock1_tmplock_,mode1);			\
	} while (0)

int evthread_is_debug_lock_held_(void *lock);
void *evthread_debug_get_real_lock_(void *lock);

void *evthread_setup_global_lock_(void *lock_, unsigned locktype,
								  int enable_locks);

#define EVTHREAD_SETUP_GLOBAL_LOCK(lockvar, locktype)			\
	do {								\
		lockvar = evthread_setup_global_lock_(lockvar,		\
		    (locktype), enable_locks);				\
		if (!lockvar) {						\
			event_warn("Couldn't allocate %s", #lockvar);	\
			return -1;					\
		}							\
	} while (0);

int event_global_setup_locks_(const int enable_locks);
int evsig_global_setup_locks_(const int enable_locks);
int evutil_global_setup_locks_(const int enable_locks);
int evutil_secure_rng_global_setup_locks_(const int enable_locks);

/** Return current evthread_lock_callbacks */
struct evthread_lock_callbacks *evthread_get_lock_callbacks(void);
/** Return current evthread_condition_callbacks */
struct evthread_condition_callbacks *evthread_get_condition_callbacks(void);
/** Disable locking for internal usage (like global shutdown) */
void evthreadimpl_disable_lock_debugging_(void);

#endif //LIBEVENT_TEST_EVTHREAD_INTERNAL_H
