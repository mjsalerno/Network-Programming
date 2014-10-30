cmake_minimum_required(VERSION 2.8.4)
project(lab2)
find_package (Threads)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros")

set(COMMON_SOURCE_FILES
    common.c
    common.h)

set(XTCP_SOURCE_FILES
    xtcp.c
    xtcp.h
    debug.h)

set(SERVER_SOURCE_FILES
    server.c
    server.h
    debug.h)

set(CLIENT_SOURCE_FILES
   client.c
   client.h)

set(IFFY_SOURCE_FILES
    get_ifi_info_plus.c
    config.h
    unp.h
    unpifiplus.h)

set(RTT_SOURCE_FILES
    rtt.c
    rtt.h)

add_library(commonlib OBJECT ${COMMON_SOURCE_FILES})
add_library(xtcplib OBJECT ${XTCP_SOURCE_FILES})
add_library(iffylib OBJECT ${IFFY_SOURCE_FILES})
add_library(rttlib OBJECT ${RTT_SOURCE_FILES})

add_executable(server
	${SERVER_SOURCE_FILES}
	$<TARGET_OBJECTS:xtcplib>
	$<TARGET_OBJECTS:commonlib>
	$<TARGET_OBJECTS:iffylib>
	$<TARGET_OBJECTS:rttlib>)

add_executable(client
	${CLIENT_SOURCE_FILES}
	$<TARGET_OBJECTS:xtcplib>
	$<TARGET_OBJECTS:commonlib>
	$<TARGET_OBJECTS:iffylib>)

target_link_libraries(client ${CMAKE_THREAD_LIBS_INIT})
if(UNIX)
  target_link_libraries(client m)
endif(UNIX)


