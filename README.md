Tiny C++ server for TCP and HTTP 1.1 connections. [libuv](https://github.com/libuv/libuv) empowers asynchronous IO.

CMake and modern C++ compiler are requirements.
Simple native building on Unix **TODO**:

    build.sh

Or on Windows:

    build.bat

    mkdir build
    cd build
    cmake -G "Visual Studio 14 Win64" ..

Goal is to support 3 platforms: Linux, Mac, Windows. And compilers: Clang, GCC, MSVC.

Small compiled core and header-only addons for server. No plans for HTTP2/SPDY-support, it's primarily backend server. May be SSL-encryption.

Enji roadmap is [here](https://github.com/aptakhin/enji/wiki/Roadmap).
