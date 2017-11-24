//
// Created by james on 21/11/2017.
//

#ifndef LIBEVENT_TEST_EVENT_H
#define LIBEVENT_TEST_EVENT_H

#include "event_struct.h"
#include "../event-internal.h"


struct event_base *event_base_new(void);

int
event_add(struct event *ev, const struct timeval *tv);


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
 * Callback for iterating events in an event base via event_base_foreach_event
 */
typedef int (*event_base_foreach_event_cb)(const struct event_base *, const struct event *, void *);
#endif //LIBEVENT_TEST_EVENT_H
