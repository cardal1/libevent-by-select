cmake_minimum_required(VERSION 3.8)
project(libevent_by_select)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
        sys/queue.h
        event.h
        event_struct.h
        util.h
        queue_test.c
        evthread.c
        evmap.c
        evthread-internal.h
        util-internal.h
        minheap-internal.h)
add_executable(libevent_by_select ${SOURCE_FILES})