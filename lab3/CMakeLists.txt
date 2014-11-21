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

set(ODR_API_SOURCE_FILES
    ODR_api.c
    ODR_api.h)

set(ODR_UTIL_SOURCE_FILES
    ODR_util.c
    ODR_util.h)

add_library(hwlib OBJECT ${HW_SOURCE_FILES})
add_library(odr_api_lib OBJECT ${ODR_API_SOURCE_FILES})
add_library(odr_util_lib OBJECT ${ODR_UTIL_SOURCE_FILES})

add_executable(ODR
    ${ODR_SOURCE_FILES}
    $<TARGET_OBJECTS:hwlib>
    $<TARGET_OBJECTS:odr_util_lib>)

add_executable(server
	${SERVER_SOURCE_FILES}
	$<TARGET_OBJECTS:odr_api_lib>)

add_executable(client
	${CLIENT_SOURCE_FILES}
	$<TARGET_OBJECTS:odr_api_lib>)

#add_executable(testcli
#   	testcli.c
#   	${ODR_SOURCE_FILES}
#   	$<TARGET_OBJECTS:hwlib>
#   	$<TARGET_OBJECTS:commonlib>)
