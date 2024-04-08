cmake_minimum_required(VERSION 3.25)

set(projname "Network_simulator_02")
project(${projname}
        VERSION 2.0
        DESCRIPTION "Simple network simulator using pipes and sockets"
        LANGUAGES C )

message( STATUS "CMAKE_SOURCE_DIR = [${CMAKE_SOURCE_DIR}]" )

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG  "${CMAKE_C_FLAGS_DEBUG} -Debug ")

add_executable(net367
        main.c main.h
        packet.c packet.h
        host.c host.h
        net.c net.h
        man.c man.h
        switch.c switch.h)

file(COPY p2p.config DESTINATION ${CMAKE_BINARY_DIR})
file(COPY p2p2.config DESTINATION ${CMAKE_BINARY_DIR})

