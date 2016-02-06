cmake_minimum_required(VERSION 2.8)
project(ENJI)

if (!MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

include_directories(src)
include_directories(deps/include)

find_library(LIBUV_LIBRARY libuv deps/lib)

if (MSVC)
    set(ENJI_LIBS Ws2_32.lib Iphlpapi.lib psapi.lib userenv.lib ${LIBUV_LIBRARY})
else()
    set(ENJI_LIBS ${LIBUV_LIBRARY})
endif()

set(ENJI_HEADERS src/enji/common.h src/enji/http.h src/enji/server.h)
set(ENJI_SOURCES src/enji/common.cpp src/enji/http.cpp src/enji/server.cpp deps/src/http_parser.c)

add_library(
    enji
    STATIC
    ${ENJI_SOURCES} ${ENJI_HEADERS}
)

add_executable(enjitests tests/test.cpp deps/src/gtest/gtest-all.cc)
add_executable(hello_world examples/hello_world.cpp)
add_executable(ping_pong examples/ping_pong.cpp)

target_link_libraries(enjitests enji ${ENJI_LIBS})
target_link_libraries(hello_world enji ${ENJI_LIBS})
target_link_libraries(ping_pong enji ${ENJI_LIBS})
