cmake_minimum_required(VERSION 2.8.4)
project(lab1)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-g -Wall -Werror -Wextra")
set(SOURCE_FILES)
find_package (Threads)

add_executable(client ${SOURCE_FILES} client.c)
add_executable(echoc echoc.c)
add_executable(timec timec.c)
add_executable(server server.c)
target_link_libraries (server ${CMAKE_THREAD_LIBS_INIT})
