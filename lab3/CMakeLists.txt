cmake_minimum_required(VERSION 2.8.4)
project(lab3)

#set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -Wall -Wextra -Werror -DDEBUG")

set(ODR_SOURCE_FILES
    ODR.c
    ODR.h
    debug.h)

set(SERVER_SOURCE_FILES
    server.c
    server.h
    debug.h)

set(CLIENT_SOURCE_FILES
   client.c
   client.h
   debug.h)

set(HW_SOURCE_FILES
    get_hw_addrs.c
    get_hw_addrs.h
    debug.h)

set(COMMON_SOURCE_FILES
    common.c
    common.h)

add_library(hwlib OBJECT ${HW_SOURCE_FILES})
add_library(commonlib OBJECT ${COMMON_SOURCE_FILES})

add_executable(ODR
    ${ODR_SOURCE_FILES}
    $<TARGET_OBJECTS:hwlib>)

add_executable(server
	${SERVER_SOURCE_FILES}
	$<TARGET_OBJECTS:commonlib>)

add_executable(client
	${CLIENT_SOURCE_FILES}
	$<TARGET_OBJECTS:commonlib>)

add_executable(testcli
   	testcli.c
   	${ODR_SOURCE_FILES}
   	$<TARGET_OBJECTS:hwlib>
   	$<TARGET_OBJECTS:commonlib>)
