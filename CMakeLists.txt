cmake_minimum_required(VERSION 2.8)
project(ENJI)

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

include_directories(src)
include_directories(deps/include)

link_directories(deps/lib/debug)

if (MSVC)
    find_library(LIBUV_LIBRARY_DEBUG NAMES libuv PATHS ${CMAKE_SOURCE_DIR}/deps/lib/debug)
    find_library(LIBUV_LIBRARY_RELEASE libuv PATHS ${CMAKE_SOURCE_DIR}/deps/lib/release)
else()
    find_library(LIBUV_LIBRARY_DEBUG NAMES libuv.a PATHS ${CMAKE_SOURCE_DIR}/deps/lib/debug NO_DEFAULT_PATH)
    find_library(LIBUV_LIBRARY_RELEASE libuv.a PATHS ${CMAKE_SOURCE_DIR}/deps/lib/release NO_DEFAULT_PATH)
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(LIBUV_LIBRARY ${LIBUV_LIBRARY_DEBUG})
else()
    set(LIBUV_LIBRARY ${LIBUV_LIBRARY_RELEASE})
endif()

if (MSVC)
    set(ENJI_DEPS_LIBS Ws2_32.lib Iphlpapi.lib psapi.lib userenv.lib ${LIBUV_LIBRARY})
else()
    set(ENJI_DEPS_LIBS ${LIBUV_LIBRARY})
endif()

set(ENJI_HEADERS
    src/enji/common.h
    src/enji/http.h
    src/enji/server.h
)

set(ENJI_SOURCES
    src/enji/common.cpp
    src/enji/http.cpp
    src/enji/server.cpp
    deps/src/http_parser.c
)

add_library(
    enji
    STATIC
    ${ENJI_SOURCES} ${ENJI_HEADERS}
)

add_executable(enjitests tests/test.cpp deps/src/gtest/gtest-all.cc)
add_executable(hello_world examples/hello_world.cpp)
add_executable(ping_pong examples/ping_pong.cpp)
add_executable(web_view examples/web_view.cpp)
add_executable(file_upload examples/file_upload.cpp)

add_executable(dropgram
    examples/dropgram/dropgram.cpp
    deps/src/sqlite3.c
)

set(ENJI_LIBS enji ${ENJI_DEPS_LIBS})

target_link_libraries(enjitests enji ${ENJI_LIBS})
target_link_libraries(hello_world ${ENJI_LIBS})
target_link_libraries(ping_pong ${ENJI_LIBS})
target_link_libraries(web_view ${ENJI_LIBS})
target_link_libraries(file_upload ${ENJI_LIBS})

target_link_libraries(dropgram ${ENJI_LIBS})
