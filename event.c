//
// Created by james on 25/11/2017.
//


#include <limits.h>
#include <string.h>
#include "event.h"
#include "evthread-internal.h"
#include "evmap-internal.h"
#include "log-internal.h"
#include "changelist-internal.h"
#include "util-internal.h"

extern const struct eventop selectops;

/* Array of backends in order of preference. */
static const struct eventop *eventops[] = {
        &selectops,
        NULL
};

/* Global state; deprecated */
struct event_base *event_global_current_base_ = NULL;
#define current_base event_global_current_base_

/* Global state */

static void *event_self_cbarg_ptr_ = NULL;

/* Prototypes */
static void	event_queue_insert_active(struct event_base *, struct event_callback *);
static void	event_queue_insert_active_later(struct event_base *, struct event_callback *);
static void	event_queue_insert_timeout(struct event_base *, struct event *);
static void	event_queue_insert_inserted(struct event_base *, struct event *);
static void	event_queue_remove_active(struct event_base *, struct event_callback *);
static void	event_queue_remove_active_later(struct event_base *, struct event_callback *);
static void	event_queue_remove_timeout(struct event_base *, struct event *);
static void	event_queue_remove_inserted(struct event_base *, struct event *);
static void event_queue_make_later_events_active(struct event_base *base);

static int evthread_make_base_notifiable_nolock_(struct event_base *base);
static int event_del_(struct event *ev, int blocking);


struct event_base *
event_init(void)
{
    struct event_base *base = event_base_new_with_config(NULL);

    if (base == NULL) {
        event_errx(1, "%s: Unable to construct event_base", __func__);
        return NULL;
    }

    current_base = base;

    return (base);
}

struct event_base *
event_base_new(void)
{
    struct event_base *base = NULL;
    struct event_config *cfg = event_config_new();
    if (cfg) {
        base = event_base_new_with_config(cfg);
        event_config_free(cfg);
    }
    return base;
}

/** Set 'tp' to the current time according to 'base'.  We must hold the lock
 * on 'base'.  If there is a cached time, return it.  Otherwise, use
 * clock_gettime or gettimeofday as appropriate to find out the right time.
 * Return 0 on success, -1 on failure.
 */
static int
gettime(struct event_base *base, struct timeval *tp)
{
    EVENT_BASE_ASSERT_LOCKED(base);

    if (base->tv_cache.tv_sec) {
        *tp = base->tv_cache;
        return (0);
    }

    if (evutil_gettime_monotonic_(&base->monotonic_timer, tp) == -1) {
        return -1;
    }

    if (base->last_updated_clock_diff + CLOCK_SYNC_INTERVAL
        < tp->tv_sec) {
        struct timeval tv;
        evutil_gettimeofday(&tv,NULL);
        evutil_timersub(&tv, tp, &base->tv_clock_diff);
        base->last_updated_clock_diff = tp->tv_sec;
    }

    return 0;
}

struct event_base *
event_base_new_with_config(const struct event_config *cfg)
{
    int i;
    struct event_base *base;
    int should_check_environment;

    if ((base = mm_calloc(1, sizeof(struct event_base))) == NULL) {
        event_warn("%s: calloc", __func__);
        return NULL;
    }

    if (cfg)
        base->flags = cfg->flags;

    should_check_environment =
            !(cfg && (cfg->flags & EVENT_BASE_FLAG_IGNORE_ENV));

    {
//        struct timeval tmp;
//        int precise_time =
//                cfg && (cfg->flags & EVENT_BASE_FLAG_PRECISE_TIMER);
//        int flags;
//        if (should_check_environment && !precise_time) {
//            precise_time = evutil_getenv_("EVENT_PRECISE_TIMER") != NULL;
//            base->flags |= EVENT_BASE_FLAG_PRECISE_TIMER;
//        }
//        flags = precise_time ? EV_MONOT_PRECISE : 0;
//        evutil_configure_monotonic_time_(&base->monotonic_timer, flags);
//
//        gettime(base, &tmp);
    }

    min_heap_ctor_(&base->timeheap);

    base->sig.ev_signal_pair[0] = -1;
    base->sig.ev_signal_pair[1] = -1;
    base->th_notify_fd[0] = -1;
    base->th_notify_fd[1] = -1;

    TAILQ_INIT(&base->active_later_queue);

    evmap_io_initmap_(&base->io);
    evmap_signal_initmap_(&base->sigmap);
    event_changelist_init_(&base->changelist);

    base->evbase = NULL;

    if (cfg) {
        memcpy(&base->max_dispatch_time,
               &cfg->max_dispatch_interval, sizeof(struct timeval));
        base->limit_callbacks_after_prio =
                cfg->limit_callbacks_after_prio;
    } else {
        base->max_dispatch_time.tv_sec = -1;
        base->limit_callbacks_after_prio = 1;
    }
    if (cfg && cfg->max_dispatch_callbacks >= 0) {
        base->max_dispatch_callbacks = cfg->max_dispatch_callbacks;
    } else {
        base->max_dispatch_callbacks = INT_MAX;
    }
    if (base->max_dispatch_callbacks == INT_MAX &&
        base->max_dispatch_time.tv_sec == -1)
        base->limit_callbacks_after_prio = INT_MAX;

    for (i = 0; eventops[i] && !base->evbase; i++) {
//        if (cfg != NULL) {
//            /* determine if this backend should be avoided */
//            if (event_config_is_avoided_method(cfg,
//                                               eventops[i]->name))
//                continue;
//            if ((eventops[i]->features & cfg->require_features)
//                != cfg->require_features)
//                continue;
//        }
//
//        /* also obey the environment variables */
//        if (should_check_environment &&
//            event_is_method_disabled(eventops[i]->name))
//            continue;

        base->evsel = eventops[i];

        base->evbase = base->evsel->init(base);
    }

    if (base->evbase == NULL) {
        event_warnx("%s: no event mechanism available",
                    __func__);
        base->evsel = NULL;
        event_base_free(base);
        return NULL;
    }

//    if (evutil_getenv_("EVENT_SHOW_METHOD"))
        event_msgx("libevent using: %s", base->evsel->name);

    /* allocate a single active event queue */
    if (event_base_priority_init(base, 1) < 0) {
        event_base_free(base);
        return NULL;
    }

    /* prepare for threading */


//#ifndef EVENT__DISABLE_THREAD_SUPPORT
    if (EVTHREAD_LOCKING_ENABLED() &&
        (!cfg || !(cfg->flags & EVENT_BASE_FLAG_NOLOCK))) {
        int r;
        EVTHREAD_ALLOC_LOCK(base->th_base_lock, 0);
        EVTHREAD_ALLOC_COND(base->current_event_cond);
        r = evthread_make_base_notifiable(base);
        if (r<0) {
            event_warnx("%s: Unable to make base notifiable.", __func__);
            event_base_free(base);
            return NULL;
        }
    }
//#endif

    return (base);
}

int
evthread_make_base_notifiable(struct event_base *base)
{
    int r;
    if (!base)
        return -1;

    EVBASE_ACQUIRE_LOCK(base, th_base_lock);
    r = evthread_make_base_notifiable_nolock_(base);
    EVBASE_RELEASE_LOCK(base, th_base_lock);
    return r;
}


static inline struct event *
event_callback_to_event(struct event_callback *evcb)
{
    EVUTIL_ASSERT((evcb->evcb_flags & EVLIST_INIT));
    return EVUTIL_UPCAST(evcb, struct event, ev_evcallback);
}

static inline struct event_callback *
event_to_event_callback(struct event *ev)
{
    return &ev->ev_evcallback;
}

int
event_add(struct event *ev, const struct timeval *tv)
{
    int res;

    if (EVUTIL_FAILURE_CHECK(!ev->ev_base)) {
        event_warnx("%s: event has no event_base set.", __func__);
        return -1;
    }

    EVBASE_ACQUIRE_LOCK(ev->ev_base, th_base_lock);

    res = event_add_nolock_(ev, tv, 0);

    EVBASE_RELEASE_LOCK(ev->ev_base, th_base_lock);

    return (res);
}


/* Implementation function to add an event.  Works just like event_add,
 * except: 1) it requires that we have the lock.  2) if tv_is_absolute is set,
 * we treat tv as an absolute time, not as an interval to add to the current
 * time */
int
event_add_nolock_(struct event *ev, const struct timeval *tv,
                  int tv_is_absolute)
{
    struct event_base *base = ev->ev_base;
    int res = 0;
    int notify = 0;

//    EVENT_BASE_ASSERT_LOCKED(base);
//    event_debug_assert_is_setup_(ev);
//
//    event_debug((
//                        "event_add: event: %p (fd "EV_SOCK_FMT"), %s%s%s%scall %p",
//            ev,
//            EV_SOCK_ARG(ev->ev_fd),
//            ev->ev_events & EV_READ ? "EV_READ " : " ",
//            ev->ev_events & EV_WRITE ? "EV_WRITE " : " ",
//            ev->ev_events & EV_CLOSED ? "EV_CLOSED " : " ",
//            tv ? "EV_TIMEOUT " : " ",
//            ev->ev_callback));
//
//    EVUTIL_ASSERT(!(ev->ev_flags & ~EVLIST_ALL));

    if (ev->ev_flags & EVLIST_FINALIZING) {
        /* XXXX debug */
        return (-1);
    }

    /*
     * prepare for timeout insertion further below, if we get a
     * failure on any step, we should not change any state.
     */
    if (tv != NULL && !(ev->ev_flags & EVLIST_TIMEOUT)) {
        if (min_heap_reserve_(&base->timeheap,
                              1 + min_heap_size_(&base->timeheap)) == -1)
            return (-1);  /* ENOMEM == errno */
    }

    /* If the main thread is currently executing a signal event's
     * callback, and we are not the main thread, then we want to wait
     * until the callback is done before we mess with the event, or else
     * we can race on ev_ncalls and ev_pncalls below. */
#ifndef EVENT__DISABLE_THREAD_SUPPORT
    if (base->current_event == event_to_event_callback(ev) &&
        (ev->ev_events & EV_SIGNAL)
        && !EVBASE_IN_THREAD(base)) {
        ++base->current_event_waiters;
        EVTHREAD_COND_WAIT(base->current_event_cond, base->th_base_lock);
    }
#endif

    if ((ev->ev_events & (EV_READ|EV_WRITE|EV_CLOSED|EV_SIGNAL)) &&
        !(ev->ev_flags & (EVLIST_INSERTED|EVLIST_ACTIVE|EVLIST_ACTIVE_LATER))) {
        if (ev->ev_events & (EV_READ|EV_WRITE|EV_CLOSED))
            res = evmap_io_add_(base, ev->ev_fd, ev);
        else if (ev->ev_events & EV_SIGNAL)
            res = evmap_signal_add_(base, (int)ev->ev_fd, ev);
        if (res != -1)
            event_queue_insert_inserted(base, ev);
        if (res == 1) {
            /* evmap says we need to notify the main thread. */
            notify = 1;
            res = 0;
        }
    }

    /*
     * we should change the timeout state only if the previous event
     * addition succeeded.
     */
    if (res != -1 && tv != NULL) {
        struct timeval now;
        int common_timeout;
#ifdef USE_REINSERT_TIMEOUT
        int was_common;
		int old_timeout_idx;
#endif

        /*
         * for persistent timeout events, we remember the
         * timeout value and re-add the event.
         *
         * If tv_is_absolute, this was already set.
         */
        if (ev->ev_closure == EV_CLOSURE_EVENT_PERSIST && !tv_is_absolute)
            ev->ev_io_timeout = *tv;

#ifndef USE_REINSERT_TIMEOUT
        if (ev->ev_flags & EVLIST_TIMEOUT) {
            event_queue_remove_timeout(base, ev);
        }
#endif

        /* Check if it is active due to a timeout.  Rescheduling
         * this timeout before the callback can be executed
         * removes it from the active list. */
        if ((ev->ev_flags & EVLIST_ACTIVE) &&
            (ev->ev_res & EV_TIMEOUT)) {
            if (ev->ev_events & EV_SIGNAL) {
                /* See if we are just active executing
                 * this event in a loop
                 */
                if (ev->ev_ncalls && ev->ev_pncalls) {
                    /* Abort loop */
                    *ev->ev_pncalls = 0;
                }
            }

            event_queue_remove_active(base, event_to_event_callback(ev));
        }

        gettime(base, &now);

        common_timeout = is_common_timeout(tv, base);
#ifdef USE_REINSERT_TIMEOUT
        was_common = is_common_timeout(&ev->ev_timeout, base);
		old_timeout_idx = COMMON_TIMEOUT_IDX(&ev->ev_timeout);
#endif

        if (tv_is_absolute) {
            ev->ev_timeout = *tv;
        } else if (common_timeout) {
            struct timeval tmp = *tv;
            tmp.tv_usec &= MICROSECONDS_MASK;
            evutil_timeradd(&now, &tmp, &ev->ev_timeout);
            ev->ev_timeout.tv_usec |=
                    (tv->tv_usec & ~MICROSECONDS_MASK);
        } else {
            evutil_timeradd(&now, tv, &ev->ev_timeout);
        }

        event_debug((
                            "event_add: event %p, timeout in %d seconds %d useconds, call %p",
                                    ev, (int)tv->tv_sec, (int)tv->tv_usec, ev->ev_callback));

#ifdef USE_REINSERT_TIMEOUT
        event_queue_reinsert_timeout(base, ev, was_common, common_timeout, old_timeout_idx);
#else
        event_queue_insert_timeout(base, ev);
#endif

        if (common_timeout) {
            struct common_timeout_list *ctl =
                    get_common_timeout_list(base, &ev->ev_timeout);
            if (ev == TAILQ_FIRST(&ctl->events)) {
                common_timeout_schedule(ctl, &now, ev);
            }
        } else {
            struct event* top = NULL;
            /* See if the earliest timeout is now earlier than it
             * was before: if so, we will need to tell the main
             * thread to wake up earlier than it would otherwise.
             * We double check the timeout of the top element to
             * handle time distortions due to system suspension.
             */
            if (min_heap_elt_is_top_(ev))
                notify = 1;
            else if ((top = min_heap_top_(&base->timeheap)) != NULL &&
                     evutil_timercmp(&top->ev_timeout, &now, <))
            notify = 1;
        }
    }

    /* if we are not in the right thread, we need to wake up the loop */
    if (res != -1 && notify && EVBASE_NEED_NOTIFY(base))
        evthread_notify_base(base);

    event_debug_note_add_(ev);

    return (res);
}

static void
event_queue_remove_inserted(struct event_base *base, struct event *ev)
{
    EVENT_BASE_ASSERT_LOCKED(base);
    if (EVUTIL_FAILURE_CHECK(!(ev->ev_flags & EVLIST_INSERTED))) {
        event_errx(1, "%s: %p(fd "EV_SOCK_FMT") not on queue %x", __func__,
                ev, EV_SOCK_ARG(ev->ev_fd), EVLIST_INSERTED);
        return;
    }
    DECR_EVENT_COUNT(base, ev->ev_flags);
    ev->ev_flags &= ~EVLIST_INSERTED;
}

struct event_config *
event_config_new(void)
{
    struct event_config *cfg = mm_calloc(1, sizeof(*cfg));

    if (cfg == NULL)
        return (NULL);

    TAILQ_INIT(&cfg->entries);
    cfg->max_dispatch_interval.tv_sec = -1;
    cfg->max_dispatch_callbacks = INT_MAX;
    cfg->limit_callbacks_after_prio = 1;

    return (cfg);
}

static void
event_config_entry_free(struct event_config_entry *entry)
{
    if (entry->avoid_method != NULL)
        mm_free((char *)entry->avoid_method);
    mm_free(entry);
}

void
event_config_free(struct event_config *cfg)
{
    struct event_config_entry *entry;

    while ((entry = TAILQ_FIRST(&cfg->entries)) != NULL) {
        TAILQ_REMOVE(&cfg->entries, entry, next);
        event_config_entry_free(entry);
    }
    mm_free(cfg);
}

int
event_base_priority_init(struct event_base *base, int npriorities)
{
    int i, r;
    r = -1;

    EVBASE_ACQUIRE_LOCK(base, th_base_lock);

    if (N_ACTIVE_CALLBACKS(base) || npriorities < 1
        || npriorities >= EVENT_MAX_PRIORITIES)
        goto err;

    if (npriorities == base->nactivequeues)
        goto ok;

    if (base->nactivequeues) {
        mm_free(base->activequeues);
        base->nactivequeues = 0;
    }

    /* Allocate our priority queues */
    base->activequeues = (struct evcallback_list *)
            mm_calloc(npriorities, sizeof(struct evcallback_list));
    if (base->activequeues == NULL) {
        event_warn("%s: calloc", __func__);
        goto err;
    }
    base->nactivequeues = npriorities;

    for (i = 0; i < base->nactivequeues; ++i) {
        TAILQ_INIT(&base->activequeues[i]);
    }

    ok:
    r = 0;
    err:
    EVBASE_RELEASE_LOCK(base, th_base_lock);
    return (r);
}

int
event_base_get_npriorities(struct event_base *base)
{

    int n;
    if (base == NULL)
        base = current_base;

    EVBASE_ACQUIRE_LOCK(base, th_base_lock);
    n = base->nactivequeues;
    EVBASE_RELEASE_LOCK(base, th_base_lock);
    return (n);
}

void
event_base_free(struct event_base *base)
{
    event_base_free_(base, 1);
}

static void
event_base_free_(struct event_base *base, int run_finalizers)
{
    int i, n_deleted=0;
    struct event *ev;
    /* XXXX grab the lock? If there is contention when one thread frees
     * the base, then the contending thread will be very sad soon. */

    /* event_base_free(NULL) is how to free the current_base if we
     * made it with event_init and forgot to hold a reference to it. */
    if (base == NULL && current_base)
        base = current_base;
    /* Don't actually free NULL. */
    if (base == NULL) {
        event_warnx("%s: no base to free", __func__);
        return;
    }
    /* XXX(niels) - check for internal events first */

#ifdef _WIN32
    event_base_stop_iocp_(base);
#endif

    /* threading fds if we have them */
    if (base->th_notify_fd[0] != -1) {
        event_del(&base->th_notify);
        EVUTIL_CLOSESOCKET(base->th_notify_fd[0]);
        if (base->th_notify_fd[1] != -1)
            EVUTIL_CLOSESOCKET(base->th_notify_fd[1]);
        base->th_notify_fd[0] = -1;
        base->th_notify_fd[1] = -1;
        event_debug_unassign(&base->th_notify);
    }

    /* Delete all non-internal events. */
    evmap_delete_all_(base);

    while ((ev = min_heap_top_(&base->timeheap)) != NULL) {
        event_del(ev);
        ++n_deleted;
    }
    for (i = 0; i < base->n_common_timeouts; ++i) {
        struct common_timeout_list *ctl =
                base->common_timeout_queues[i];
        event_del(&ctl->timeout_event); /* Internal; doesn't count */
        event_debug_unassign(&ctl->timeout_event);
        for (ev = TAILQ_FIRST(&ctl->events); ev; ) {
            struct event *next = TAILQ_NEXT(ev,
                                            ev_timeout_pos.ev_next_with_common_timeout);
            if (!(ev->ev_flags & EVLIST_INTERNAL)) {
                event_del(ev);
                ++n_deleted;
            }
            ev = next;
        }
        mm_free(ctl);
    }
    if (base->common_timeout_queues)
        mm_free(base->common_timeout_queues);

    for (;;) {
        /* For finalizers we can register yet another finalizer out from
         * finalizer, and iff finalizer will be in active_later_queue we can
         * add finalizer to activequeues, and we will have events in
         * activequeues after this function returns, which is not what we want
         * (we even have an assertion for this).
         *
         * A simple case is bufferevent with underlying (i.e. filters).
         */
        int i = event_base_free_queues_(base, run_finalizers);
        if (!i) {
            break;
        }
        n_deleted += i;
    }

    if (n_deleted)
        event_debug(("%s: %d events were still set in base",
                __func__, n_deleted));

    while (LIST_FIRST(&base->once_events)) {
        struct event_once *eonce = LIST_FIRST(&base->once_events);
        LIST_REMOVE(eonce, next_once);
        mm_free(eonce);
    }

    if (base->evsel != NULL && base->evsel->dealloc != NULL)
        base->evsel->dealloc(base);

    for (i = 0; i < base->nactivequeues; ++i)
        EVUTIL_ASSERT(TAILQ_EMPTY(&base->activequeues[i]));

    EVUTIL_ASSERT(min_heap_empty_(&base->timeheap));
    min_heap_dtor_(&base->timeheap);

    mm_free(base->activequeues);

    evmap_io_clear_(&base->io);
    evmap_signal_clear_(&base->sigmap);
    event_changelist_freemem_(&base->changelist);

    EVTHREAD_FREE_LOCK(base->th_base_lock, 0);
    EVTHREAD_FREE_COND(base->current_event_cond);

    /* If we're freeing current_base, there won't be a current_base. */
    if (base == current_base)
        current_base = NULL;
    mm_free(base);
}


static int
evthread_make_base_notifiable_nolock_(struct event_base *base)
{
    void (*cb)(evutil_socket_t, short, void *);
    int (*notify)(struct event_base *);

    if (base->th_notify_fn != NULL) {
        /* The base is already notifiable: we're doing fine. */
        return 0;
    }

#if defined(EVENT__HAVE_WORKING_KQUEUE)
    if (base->evsel == &kqops && event_kq_add_notify_event_(base) == 0) {
		base->th_notify_fn = event_kq_notify_base_;
		/* No need to add an event here; the backend can wake
		 * itself up just fine. */
		return 0;
	}
#endif

#ifdef EVENT__HAVE_EVENTFD
    base->th_notify_fd[0] = evutil_eventfd_(0,
	    EVUTIL_EFD_CLOEXEC|EVUTIL_EFD_NONBLOCK);
	if (base->th_notify_fd[0] >= 0) {
		base->th_notify_fd[1] = -1;
		notify = evthread_notify_base_eventfd;
		cb = evthread_notify_drain_eventfd;
	} else
#endif
    if (evutil_make_internal_pipe_(base->th_notify_fd) == 0) {
        notify = evthread_notify_base_default;
        cb = evthread_notify_drain_default;
    } else {
        return -1;
    }

    base->th_notify_fn = notify;

    /* prepare an event that we can use for wakeup */
    event_assign(&base->th_notify, base, base->th_notify_fd[0],
                 EV_READ|EV_PERSIST, cb, base);

    /* we need to mark this as internal event */
    base->th_notify.ev_flags |= EVLIST_INTERNAL;
    event_priority_set(&base->th_notify, 0);

    return event_add_nolock_(&base->th_notify, NULL, 0);
}


int
event_dispatch(void)
{
    return (event_loop(0));
}

int
event_base_dispatch(struct event_base *event_base)
{
    return (event_base_loop(event_base, 0));
}

/* not thread safe */

int
event_loop(int flags)
{
    return event_base_loop(current_base, flags);
}

/** Make 'base' have no current cached time. */
static inline void
clear_time_cache(struct event_base *base)
{
    base->tv_cache.tv_sec = 0;
}

int
event_base_loop(struct event_base *base, int flags)
{
    const struct eventop *evsel = base->evsel;
    struct timeval tv;
    struct timeval *tv_p;
    int res, done, retval = 0;

    /* Grab the lock.  We will release it inside evsel.dispatch, and again
     * as we invoke user callbacks. */
    EVBASE_ACQUIRE_LOCK(base, th_base_lock);

    if (base->running_loop) {
        event_warnx("%s: reentrant invocation.  Only one event_base_loop"
                            " can run on each event_base at once.", __func__);
        EVBASE_RELEASE_LOCK(base, th_base_lock);
        return -1;
    }

    base->running_loop = 1;

    clear_time_cache(base);

    if (base->sig.ev_signal_added && base->sig.ev_n_signals_added)
        evsig_set_base_(base);

    done = 0;

#ifndef EVENT__DISABLE_THREAD_SUPPORT
    base->th_owner_id = EVTHREAD_GET_ID();
#endif

    base->event_gotterm = base->event_break = 0;

    while (!done) {
        base->event_continue = 0;
        base->n_deferreds_queued = 0;

        /* Terminate the loop if we have been asked to */
        if (base->event_gotterm) {
            break;
        }

        if (base->event_break) {
            break;
        }

        tv_p = &tv;
        if (!N_ACTIVE_CALLBACKS(base) && !(flags & EVLOOP_NONBLOCK)) {
            timeout_next(base, &tv_p);
        } else {
            /*
             * if we have active events, we just poll new events
             * without waiting.
             */
            evutil_timerclear(&tv);
        }

        /* If we have no events, we just exit */
        if (0==(flags&EVLOOP_NO_EXIT_ON_EMPTY) &&
            !event_haveevents(base) && !N_ACTIVE_CALLBACKS(base)) {
            event_debug(("%s: no events registered.", __func__));
            retval = 1;
            goto done;
        }

        event_queue_make_later_events_active(base);

        clear_time_cache(base);

        res = evsel->dispatch(base, tv_p);

        if (res == -1) {
            event_debug(("%s: dispatch returned unsuccessfully.",
                    __func__));
            retval = -1;
            goto done;
        }

        update_time_cache(base);

        timeout_process(base);

        if (N_ACTIVE_CALLBACKS(base)) {
            int n = event_process_active(base);
            if ((flags & EVLOOP_ONCE)
                && N_ACTIVE_CALLBACKS(base) == 0
                && n != 0)
                done = 1;
        } else if (flags & EVLOOP_NONBLOCK)
            done = 1;
    }
    event_debug(("%s: asked to terminate loop.", __func__));

    done:
    clear_time_cache(base);
    base->running_loop = 0;

    EVBASE_RELEASE_LOCK(base, th_base_lock);

    return (retval);
}


static int
timeout_next(struct event_base *base, struct timeval **tv_p)
{
    /* Caller must hold th_base_lock */
    struct timeval now;
    struct event *ev;
    struct timeval *tv = *tv_p;
    int res = 0;

    ev = min_heap_top_(&base->timeheap);

    if (ev == NULL) {
        /* if no time-based events are active wait for I/O */
        *tv_p = NULL;
        goto out;
    }

    if (gettime(base, &now) == -1) {
        res = -1;
        goto out;
    }

    if (evutil_timercmp(&ev->ev_timeout, &now, <=)) {
        evutil_timerclear(tv);
        goto out;
    }

    evutil_timersub(&ev->ev_timeout, &now, tv);

    EVUTIL_ASSERT(tv->tv_sec >= 0);
    EVUTIL_ASSERT(tv->tv_usec >= 0);
    event_debug(("timeout_next: event: %p, in %d seconds, %d useconds", ev, (int)tv->tv_sec, (int)tv->tv_usec));

    out:
    return (res);
}

static void
event_queue_make_later_events_active(struct event_base *base)
{
    struct event_callback *evcb;
    EVENT_BASE_ASSERT_LOCKED(base);

    while ((evcb = TAILQ_FIRST(&base->active_later_queue))) {
        TAILQ_REMOVE(&base->active_later_queue, evcb, evcb_active_next);
        evcb->evcb_flags = (evcb->evcb_flags & ~EVLIST_ACTIVE_LATER) | EVLIST_ACTIVE;
        EVUTIL_ASSERT(evcb->evcb_pri < base->nactivequeues);
        TAILQ_INSERT_TAIL(&base->activequeues[evcb->evcb_pri], evcb, evcb_active_next);
        base->n_deferreds_queued += (evcb->evcb_closure == EV_CLOSURE_CB_SELF);
    }
}