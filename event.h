//
// Created by james on 21/11/2017.
//

#ifndef LIBEVENT_TEST_EVENT_H
#define LIBEVENT_TEST_EVENT_H

#include "event_struct.h"


struct event_base *event_base_new(void);

int
event_add(struct event *ev, const struct timeval *tv);

int event_base_dispatch(struct event_base *);

/** @name Loop flags

    These flags control the behavior of event_base_loop().
 */
/**@{*/
/** Block until we have an active event, then exit once all active events
 * have had their callbacks run. */
#define EVLOOP_ONCE	0x01
/** Do not block: see which events are ready now, run the callbacks
 * of the highest-priority ones, then exit. */
#define EVLOOP_NONBLOCK	0x02
/** Do not exit the loop because we have no pending events.  Instead, keep
 * running until event_base_loopexit() or event_base_loopbreak() makes us
 * stop.
 */
#define EVLOOP_NO_EXIT_ON_EMPTY 0x04
/**
 * @name event flags
 *
 * Flags to pass to event_new(), event_assign(), event_pending(), and
 * anything else with an argument of the form "short events"
 */
/**@{*/
/** Indicates that a timeout has occurred.  It's not necessary to pass
 * this flag to event_for new()/event_assign() to get a timeout. */
#define EV_TIMEOUT	0x01
/** Wait for a socket or FD to become readable */
#define EV_READ		0x02
/** Wait for a socket or FD to become writeable */
#define EV_WRITE	0x04
/** Wait for a POSIX signal to be raised*/
#define EV_SIGNAL	0x08
/**
 * Persistent event: won't get removed automatically when activated.
 *
 * When a persistent event with a timeout becomes activated, its timeout
 * is reset to 0.
 */
#define EV_PERSIST	0x10
/** Select edge-triggered behavior, if supported by the backend. */
#define EV_ET		0x20
/**
 * If this option is provided, then event_del() will not block in one thread
 * while waiting for the event callback to complete in another thread.
 *
 * To use this option safely, you may need to use event_finalize() or
 * event_free_finalize() in order to safely tear down an event in a
 * multithreaded application.  See those functions for more information.
 *
 * THIS IS AN EXPERIMENTAL API. IT MIGHT CHANGE BEFORE THE LIBEVENT 2.1 SERIES
 * BECOMES STABLE.
 **/
#define EV_FINALIZE     0x40
/**
 * Detects connection close events.  You can use this to detect when a
 * connection has been closed, without having to read all the pending data
 * from a connection.
 *
 * Not all backends support EV_CLOSED.  To detect or require it, use the
 * feature flag EV_FEATURE_EARLY_CLOSE.
 **/
#define EV_CLOSED	0x80
/**@}*/

/**
   A flag used to describe which features an event_base (must) provide.

   Because of OS limitations, not every Libevent backend supports every
   possible feature.  You can use this type with
   event_config_require_features() to tell Libevent to only proceed if your
   event_base implements a given feature, and you can receive this type from
   event_base_get_features() to see which features are available.
*/
enum event_method_feature {
    /** Require an event method that allows edge-triggered events with EV_ET. */
            EV_FEATURE_ET = 0x01,
    /** Require an event method where having one event triggered among
     * many is [approximately] an O(1) operation. This excludes (for
     * example) select and poll, which are approximately O(N) for N
     * equal to the total number of possible events. */
            EV_FEATURE_O1 = 0x02,
    /** Require an event method that allows file descriptors as well as
     * sockets. */
            EV_FEATURE_FDS = 0x04,
    /** Require an event method that allows you to use EV_CLOSED to detect
     * connection close without the necessity of reading all the pending data.
     *
     * Methods that do support EV_CLOSED may not be able to provide support on
     * all kernel versions.
     **/
            EV_FEATURE_EARLY_CLOSE = 0x08
};

/**
   A flag passed to event_config_set_flag().

    These flags change the behavior of an allocated event_base.

    @see event_config_set_flag(), event_base_new_with_config(),
       event_method_feature
 */
enum event_base_config_flag {
    /** Do not allocate a lock for the event base, even if we have
        locking set up.

        Setting this option will make it unsafe and nonfunctional to call
        functions on the base concurrently from multiple threads.
    */
            EVENT_BASE_FLAG_NOLOCK = 0x01,
    /** Do not check the EVENT_* environment variables when configuring
        an event_base  */
            EVENT_BASE_FLAG_IGNORE_ENV = 0x02,
    /** Windows only: enable the IOCP dispatcher at startup

        If this flag is set then bufferevent_socket_new() and
        evconn_listener_new() will use IOCP-backed implementations
        instead of the usual select-based one on Windows.
     */
            EVENT_BASE_FLAG_STARTUP_IOCP = 0x04,
    /** Instead of checking the current time every time the event loop is
        ready to run timeout callbacks, check after each timeout callback.
     */
            EVENT_BASE_FLAG_NO_CACHE_TIME = 0x08,

    /** If we are using the epoll backend, this flag says that it is
        safe to use Libevent's internal change-list code to batch up
        adds and deletes in order to try to do as few syscalls as
        possible.  Setting this flag can make your code run faster, but
        it may trigger a Linux bug: it is not safe to use this flag
        if you have any fds cloned by dup() or its variants.  Doing so
        will produce strange and hard-to-diagnose bugs.

        This flag can also be activated by setting the
        EVENT_EPOLL_USE_CHANGELIST environment variable.

        This flag has no effect if you wind up using a backend other than
        epoll.
     */
            EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST = 0x10,

    /** Ordinarily, Libevent implements its time and timeout code using
        the fastest monotonic timer that we have.  If this flag is set,
        however, we use less efficient more precise timer, assuming one is
        present.
     */
            EVENT_BASE_FLAG_PRECISE_TIMER = 0x20
};
/**
 * Callback for iterating events in an event base via event_base_foreach_event
 */
typedef int (*event_base_foreach_event_cb)(const struct event_base *, const struct event *, void *);
#endif //LIBEVENT_TEST_EVENT_H
