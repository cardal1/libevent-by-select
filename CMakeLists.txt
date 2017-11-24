cmake_minimum_required(VERSION 3.8)
project(libevent_by_select)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
        sys/queue.h
        event.h
        event_struct.h
        util.h
        main.c)
add_executable(libevent_by_select ${SOURCE_FILES})