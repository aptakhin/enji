cmake_minimum_required(VERSION 2.8)
project(ENJI)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake) #CLion
    include(${CMAKE_CURRENT_SOURCE_DIR}/build/conanbuildinfo.cmake)
else()
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) # Not CLion
endif()

conan_basic_setup()

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

include_directories(src)

set(ENJI_HEADERS
    src/enji/common.h
    src/enji/http.h
    src/enji/server.h
)

set(ENJI_SOURCES
    src/enji/common.cpp
    src/enji/http.cpp
    src/enji/server.cpp
)

add_library(enji
    STATIC
    ${ENJI_SOURCES} ${ENJI_HEADERS}
)

add_executable(enjitests tests/test.cpp)
add_executable(hello_world examples/hello_world.cpp)
add_executable(ping_pong examples/ping_pong.cpp)
add_executable(web_view examples/web_view.cpp)
add_executable(file_upload examples/file_upload.cpp)

add_executable(dropgram examples/dropgram/dropgram.cpp)

set(ENJI_LIBS enji ${CONAN_LIBS})

target_link_libraries(enjitests enji ${ENJI_LIBS})
target_link_libraries(hello_world ${ENJI_LIBS})
target_link_libraries(ping_pong ${ENJI_LIBS})
target_link_libraries(web_view ${ENJI_LIBS})
target_link_libraries(file_upload ${ENJI_LIBS})

target_link_libraries(dropgram ${ENJI_LIBS})
