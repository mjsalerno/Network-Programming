cmake_minimum_required(VERSION 2.8.4)
project(lab2)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros")

set(COMMON_SOURCE_FILES
    common.c
    common.h)

set(XTCP_SOURCE_FILES
    xtcp.c
    xtcp.h)

set(SERVER_SOURCE_FILES
    server.c
    server.h)

set(CLIENT_SOURCE_FILES
   client.c
   client.h)

add_library(commonlib OBJECT ${COMMON_SOURCE_FILES})
#add_library(xtcp OBJECT ${XTCP_SOURCE_FILES})

add_executable(server
	${SERVER_SOURCE_FILES}
	$<TARGET_OBJECTS:commonlib>)

add_executable(client
	${CLIENT_SOURCE_FILES}
	$<TARGET_OBJECTS:commonlib>)

