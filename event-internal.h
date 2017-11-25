//
// Created by james on 23/11/2017.
//

#ifndef EVENT_INTERNAL_H_INCLUDED_
#define EVENT_INTERNAL_H_INCLUDED_

#include "minheap-internal.h"
#include "event.h"
#include "evsignal-internal.h"

/* mutually exclusive */
#define ev_signal_next	ev_.ev_signal.ev_signal_next
#define ev_io_next	ev_.ev_io.ev_io_next
#define ev_io_timeout	ev_.ev_io.ev_timeout

/* used only by signals */
#define ev_ncalls	ev_.ev_signal.ev_ncalls
#define ev_pncalls	ev_.ev_signal.ev_pncalls

#define ev_pri ev_evcallback.evcb_pri
#define ev_flags ev_evcallback.evcb_flags
#define ev_closure ev_evcallback.evcb_closure
#define ev_callback ev_evcallback.evcb_cb_union.evcb_callback
#define ev_arg ev_evcallback.evcb_arg

/** Structure to define the backend of a given event_base. */
struct eventop {
    /** The name of this backend. */
    const char *name;
    /** Function to set up an event_base to use this backend.  It should
     * create a new structure holding whatever information is needed to
     * run the backend, and return it.  The returned pointer will get
     * stored by event_init into the event_base.evbase field.  On failure,
     * this function should return NULL. */
    void *(*init)(struct event_base *);
    /** Enable reading/writing on a given fd or signal.  'events' will be
     * the events that we're trying to enable: one or more of EV_READ,
     * EV_WRITE, EV_SIGNAL, and EV_ET.  'old' will be those events that
     * were enabled on this fd previously.  'fdinfo' will be a structure
     * associated with the fd by the evmap; its size is defined by the
     * fdinfo field below.  It will be set to 0 the first time the fd is
     * added.  The function should return 0 on success and -1 on error.
     */
    int (*add)(struct event_base *, evutil_socket_t fd, short old, short events, void *fdinfo);
    /** As "add", except 'events' contains the events we mean to disable. */
    int (*del)(struct event_base *, evutil_socket_t fd, short old, short events, void *fdinfo);
    /** Function to implement the core of an event loop.  It must see which
        added events are ready, and cause event_active to be called for each
        active event (usually via event_io_active or such).  It should
        return 0 on success and -1 on error.
     */
    int (*dispatch)(struct event_base *, struct timeval *);
    /** Function to clean up and free our data from the event_base. */
    void (*dealloc)(struct event_base *);
    /** Flag: set if we need to reinitialize the event base after we fork.
     */
    int need_reinit;
    /** Bit-array of supported event_method_features that this backend can
     * provide. */
//    enum event_method_feature features;
    /** Length of the extra information we should record for each fd that
        has one or more active events.  This information is recorded
        as part of the evmap entry for each fd, and passed as an argument
        to the add and del functions above.
     */
    size_t fdinfo_len;
};

#define event_io_map event_signal_map

/* Used to map signal numbers to a list of events.  If EVMAP_USE_HT is not
   defined, this structure is also used as event_io_map, which maps fds to a
   list of events.
*/
struct event_signal_map {
    /* An array of evmap_io * or of evmap_signal *; empty entries are
     * set to NULL. */
    void **entries;
    /* The number of entries available in entries */
    int nentries;
};


struct event_base {
    /** Function pointers and other data to describe this event_base's
     * backend. */
    const struct eventop *evsel;
    /** Pointer to backend-specific data. */
    void *evbase;

    /** Function pointers used to describe the backend that this event_base
 * uses for signals */
    const struct eventop *evsigsel;
    /** Data to implement the common signal handelr code. */
    struct evsig_info sig;

    /** Mapping from file descriptors to enabled (added) events */
    struct event_io_map io;


    /** Mapping from signal numbers to enabled (added) events. */
    struct event_signal_map sigmap;

    /* threading support */
    /** The thread currently running the event_loop for this base */
    unsigned long th_owner_id;
    /** A lock to prevent conflicting accesses to this event_base */
    void *th_base_lock;
    /** A condition that gets signalled when we're done processing an
     * event with waiters on it. */
    void *current_event_cond;
    /** Number of threads blocking on current_event_cond. */
    int current_event_waiters;

    /** The event whose callback is executing right now */
    struct event_callback *current_event;

    /** Priority queue of events with timeouts. */
    struct min_heap timeheap;
};

#endif /* EVENT_INTERNAL_H_INCLUDED_ */
