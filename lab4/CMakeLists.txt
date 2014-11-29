cmake_minimum_required(VERSION 2.8.4)
project(lab4)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -Wextra")

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

add_library(ping_lib OBJECT ${PING_SRC_FILES})
add_library(common_lib OBJECT ${COMMON_SRC_FILES})
add_library(hwa_lib OBJECT ${HWA_SRC_FILES})


add_executable(test
    ${TEST_SRC_FILES}
    $<TARGET_OBJECTS:ping_lib>
    $<TARGET_OBJECTS:common_lib>)

add_executable(ARP
    ${ARP_SRC_FILES}
    $<TARGET_OBJECTS:hwa_lib>
    $<TARGET_OBJECTS:common_lib>)