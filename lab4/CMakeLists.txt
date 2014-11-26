cmake_minimum_required(VERSION 2.8.4)
project(lab4)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -Wextra")

set(SOURCE_FILES
    ping.c
    ping.h)

#include_directories(.)

add_executable(ping ${SOURCE_FILES})