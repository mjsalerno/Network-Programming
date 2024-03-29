cmake_minimum_required(VERSION 2.8.4)
project(lab4)
find_package(Threads)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -Wextra -DDEBUG")

set(TOUR_SRC_FILES
    tour.c
    tour.h
    debug.h)

set(PING_SRC_FILES
    ping.c
    ping.h
    debug.h)

set(TEST_SRC_FILES
    test.c)

set(COMMON_SRC_FILES
    common.c
    common.h
    debug.h
    api.h)

set(ARP_SRC_FILES
    ARP.c
    ARP.h
    debug.h)

set(HWA_SRC_FILES
    get_hw_addrs.c
    get_hw_addrs.h
    debug.h)

set(ARP_UTIL_SRC_FILES
    ARP_util.c
    ARP.h
    debug.h)

set(API_SRC_FILES
    api.c
    api.h
    debug.h)

add_library(ping_lib OBJECT ${PING_SRC_FILES})
add_library(common_lib OBJECT ${COMMON_SRC_FILES})
add_library(hwa_lib OBJECT ${HWA_SRC_FILES})
add_library(arp_util_lib OBJECT ${ARP_UTIL_SRC_FILES})
add_library(api_lib OBJECT ${API_SRC_FILES})

add_executable(tour
    ${TOUR_SRC_FILES}
    $<TARGET_OBJECTS:ping_lib>
    $<TARGET_OBJECTS:common_lib>
    $<TARGET_OBJECTS:api_lib>)

add_executable(test
    ${TEST_SRC_FILES}
    $<TARGET_OBJECTS:ping_lib>
    $<TARGET_OBJECTS:common_lib>
    $<TARGET_OBJECTS:arp_util_lib>
    $<TARGET_OBJECTS:api_lib>)

add_executable(ARP
    ${ARP_SRC_FILES}
    $<TARGET_OBJECTS:hwa_lib>
    $<TARGET_OBJECTS:common_lib>
    $<TARGET_OBJECTS:arp_util_lib>
    $<TARGET_OBJECTS:api_lib>)

target_link_libraries(test ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(tour ${CMAKE_THREAD_LIBS_INIT})
