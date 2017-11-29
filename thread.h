//
// Created by james on 27/11/2017.
//

#ifndef LIBEVENT_BY_SELECT_THREAD_H
#define LIBEVENT_BY_SELECT_THREAD_H

/** A recursive lock is one that can be acquired multiple times at once by the
 * same thread.  No other process can allocate the lock until the thread that
 * has been holding it has unlocked it as many times as it locked it. */
#define EVTHREAD_LOCKTYPE_RECURSIVE 1
/* A read-write lock is one that allows multiple simultaneous readers, but
 * where any one writer excludes all other writers and readers. */
#define EVTHREAD_LOCKTYPE_READWRITE 2
/**@}*/

/** This structure describes the interface a threading library uses for
 * locking.   It's used to tell evthread_set_lock_callbacks() how to use
 * locking on this platform.
 */
struct evthread_lock_callbacks {
    /** The current version of the locking API.  Set this to
     * EVTHREAD_LOCK_API_VERSION */
    int lock_api_version;
    /** Which kinds of locks does this version of the locking API
     * support?  A bitfield of EVTHREAD_LOCKTYPE_RECURSIVE and
     * EVTHREAD_LOCKTYPE_READWRITE.
     *
     * (Note that RECURSIVE locks are currently mandatory, and
     * READWRITE locks are not currently used.)
     **/
    unsigned supported_locktypes;
    /** Function to allocate and initialize new lock of type 'locktype'.
     * Returns NULL on failure. */
    void *(*alloc)(unsigned locktype);
    /** Funtion to release all storage held in 'lock', which was created
     * with type 'locktype'. */
    void (*free)(void *lock, unsigned locktype);
    /** Acquire an already-allocated lock at 'lock' with mode 'mode'.
     * Returns 0 on success, and nonzero on failure. */
    int (*lock)(unsigned mode, void *lock);
    /** Release a lock at 'lock' using mode 'mode'.  Returns 0 on success,
     * and nonzero on failure. */
    int (*unlock)(unsigned mode, void *lock);
};


int evthread_set_lock_callbacks(const struct evthread_lock_callbacks *);

#define EVTHREAD_CONDITION_API_VERSION 1

struct timeval;

/** This structure describes the interface a threading library uses for
 * condition variables.  It's used to tell evthread_set_condition_callbacks
 * how to use locking on this platform.
 */
struct evthread_condition_callbacks {
    /** The current version of the conditions API.  Set this to
     * EVTHREAD_CONDITION_API_VERSION */
    int condition_api_version;
    /** Function to allocate and initialize a new condition variable.
     * Returns the condition variable on success, and NULL on failure.
     * The 'condtype' argument will be 0 with this API version.
     */
    void *(*alloc_condition)(unsigned condtype);
    /** Function to free a condition variable. */
    void (*free_condition)(void *cond);
    /** Function to signal a condition variable.  If 'broadcast' is 1, all
     * threads waiting on 'cond' should be woken; otherwise, only on one
     * thread is worken.  Should return 0 on success, -1 on failure.
     * This function will only be called while holding the associated
     * lock for the condition.
     */
    int (*signal_condition)(void *cond, int broadcast);
    /** Function to wait for a condition variable.  The lock 'lock'
     * will be held when this function is called; should be released
     * while waiting for the condition to be come signalled, and
     * should be held again when this function returns.
     * If timeout is provided, it is interval of seconds to wait for
     * the event to become signalled; if it is NULL, the function
     * should wait indefinitely.
     *
     * The function should return -1 on error; 0 if the condition
     * was signalled, or 1 on a timeout. */
    int (*wait_condition)(void *cond, void *lock,
                          const struct timeval *timeout);
};
#endif //LIBEVENT_BY_SELECT_THREAD_H
