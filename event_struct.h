//
// Created by james on 22/11/2017.
//

#ifndef LIBEVENT_TEST_EVENT_STRUCT_H
#define LIBEVENT_TEST_EVENT_STRUCT_H

#include <sys/time.h>

#include "util.h"
#include "sys/queue.h"

#define EVLIST_TIMEOUT	    0x01
#define EVLIST_INSERTED	    0x02
#define EVLIST_SIGNAL	    0x04
#define EVLIST_ACTIVE	    0x08
#define EVLIST_INTERNAL	    0x10
#define EVLIST_ACTIVE_LATER 0x20
#define EVLIST_FINALIZING   0x40
#define EVLIST_INIT	    0x80

#define EVLIST_ALL          0xff

LIST_HEAD (event_dlist, event);

struct event_callback {
    TAILQ_ENTRY(event_callback) evcb_active_next;
    short evcb_flags;
    ev_uint8_t evcb_pri;	/* smaller numbers are higher priority */
    ev_uint8_t evcb_closure;
    /* allows us to adopt for different types of events */
    union {
        void (*evcb_callback)(evutil_socket_t, short, void *);
        void (*evcb_selfcb)(struct event_callback *, void *);
        void (*evcb_evfinalize)(struct event *, void *);
        void (*evcb_cbfinalize)(struct event_callback *, void *);
    } evcb_cb_union;
    void *evcb_arg;
};

struct event {
    struct event_callback ev_evcallback;

    /* for managing timeouts */
    union {
        TAILQ_ENTRY(event) ev_next_with_common_timeout;
        int min_heap_idx;
    } ev_timeout_pos;
    evutil_socket_t ev_fd;

    struct event_base *ev_base;

    union {
        /* used for io events */
        struct {
            LIST_ENTRY (event) ev_io_next;
            struct timeval ev_timeout;
        } ev_io;

        /* used by signal events */
        struct {
            LIST_ENTRY (event) ev_signal_next;
            short ev_ncalls;
            /* Allows deletes in callback */
            short *ev_pncalls;
        } ev_signal;
    } ev_;

    short ev_events;
    short ev_res;		/* result passed to event callback */
    struct timeval ev_timeout;
};

#endif //LIBEVENT_TEST_EVENT_STRUCT_H
