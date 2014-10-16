cmake_minimum_required(VERSION 2.8.4)
project(lab2)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -Wall -Werror -Wextra")

set(COMMON_SOURCE_FILES
    common.c
    common.h
    xtcp.c
    xtcp.h)

set(SERVER_SOURCE_FILES
    server.c
    server.h
    ${COMMON_SOURCE_FILES})

set(CLIENT_SOURCE_FILES
   client.c
   client.h
   ${COMMON_SOURCE_FILES})

add_executable(server ${SERVER_SOURCE_FILES})
add_executable(client ${SERVER_SOURCE_FILES})
