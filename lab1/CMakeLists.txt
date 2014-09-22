cmake_minimum_required(VERSION 2.8.4)
project(lab1)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-g -Wall -Werror -Wextra")

set(SOURCE_FILES)
add_executable(client ${SOURCE_FILES} client.c)