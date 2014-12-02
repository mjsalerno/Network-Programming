cmake_minimum_required(VERSION 2.8.4)
project(lab4)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -Wextra")

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
    common.h)

set(ARP_SRC_FILES
    ARP.c
    ARP.h)

set(HWA_SRC_FILES
    get_hw_addrs.c
    get_hw_addrs.h)

set(ARP_UTIL_SRC_FILES
    ARP_util.c
    ARP.h)

add_library(ping_lib OBJECT ${PING_SRC_FILES})
add_library(common_lib OBJECT ${COMMON_SRC_FILES})
add_library(hwa_lib OBJECT ${HWA_SRC_FILES})
add_library(arp_util_lib OBJECT ${ARP_UTIL_SRC_FILES})

add_executable(tour
    ${TOUR_SRC_FILES}
    $<TARGET_OBJECTS:ping_lib>
    $<TARGET_OBJECTS:common_lib>)

add_executable(test
    ${TEST_SRC_FILES}
    $<TARGET_OBJECTS:ping_lib>
    $<TARGET_OBJECTS:common_lib>)

add_executable(ARP
    ${ARP_SRC_FILES}
    $<TARGET_OBJECTS:hwa_lib>
    $<TARGET_OBJECTS:common_lib>
    $<TARGET_OBJECTS:arp_util_lib>)
