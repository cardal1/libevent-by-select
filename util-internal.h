//
// Created by james on 23/11/2017.
//

#ifndef LIBEVENT_TEST_UTIL_INTERNAL_H
#define LIBEVENT_TEST_UTIL_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>

#define EVUTIL_UNLIKELY(p) (p)

#define EVUTIL_ASSERT(cond)						\
	do {								\
		if (EVUTIL_UNLIKELY(!(cond))) {				\
			event_errx(EVENT_ERR_ABORT_,			\
			    "%s:%d: Assertion %s failed in %s",		\
			    __FILE__,__LINE__,#cond,__func__);		\
			/* In case a user-supplied handler tries to */	\
			/* return control to us, log and abort here. */	\
			(void)fprintf(stderr,				\
			    "%s:%d: Assertion %s failed in %s",		\
			    __FILE__,__LINE__,#cond,__func__);		\
			abort();					\
		}							\
	} while (0)

#endif //LIBEVENT_TEST_UTIL_INTERNAL_H
