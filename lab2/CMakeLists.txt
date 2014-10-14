cmake_minimum_required(VERSION 2.8.4)
project(lab2)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SERVER_SOURCE_FILES
    server.c
    server.h
    common.c
    common.h)

set(CLIENT_SOURCE_FILES
   client.c
   client.h
   common.c
   common.h)

add_executable(server ${SERVER_SOURCE_FILES})
add_executable(client ${SERVER_SOURCE_FILES})
