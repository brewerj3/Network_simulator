cmake_minimum_required(VERSION 3.25)

set(projname "Network_simulator_02")
project(${projname}
        VERSION 2.0
        DESCRIPTION "Simple network simulator using pipes and sockets"
        LANGUAGES C )

message( STATUS "CMAKE_SOURCE_DIR = [${CMAKE_SOURCE_DIR}]" )

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra")
#set(CMAKE_C_FLAGS_DEBUG  "${CMAKE_C_FLAGS_DEBUG} -Debug ")

add_executable(net367
        Lab07/main.c Lab07/main.h
        Lab07/host.c Lab07/host.h
        Lab07/man.c Lab07/man.h
        Lab07/net.c Lab07/net.h
        Lab07/packet.c Lab07/packet.h
        Lab07/switch.c Lab07/switch.c
        Lab07/server.c Lab07/server.h)

file(COPY p2p.config DESTINATION ${CMAKE_BINARY_DIR})
file(COPY p2p2.config DESTINATION ${CMAKE_BINARY_DIR})
file(COPY pDNSp.config DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ring.config DESTINATION ${CMAKE_BINARY_DIR})

