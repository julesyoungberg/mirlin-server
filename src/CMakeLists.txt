cmake_minimum_required (VERSION 3.5)
project(WebsocketServer)

# Set the C++ standard to C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable all compiler warnings
if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Link against pthreads under Unix systems
if (NOT MSVC AND NOT MINGW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# Under MinGW, statically link against the standard libraries
if (MINGW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/local/lib -lessentia -lfftw3 -lyaml -lavcodec -lavformat -lavutil -lsamplerate -ltag -lfftw3f -lQtCore -lgaia2")

# Force WebSocket++ to use 100% C++11 mode, so that it doesn't try to look for Boost
# (Note that under MinGW, Boost.Thread is still required, due to a bug in MinGW that prevents the C++11 version from being used)
add_definitions(-D_WEBSOCKETPP_CPP11_STRICT_)
add_definitions(-D__STDC_CONSTANT_MACROS)

# Add the third-party headers directory to the search path
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/../external")

# essentia includes
# https://github.com/MTG/essentia/blob/master/FAQ.md#how-to-compile-my-own-c-code-that-uses-essentia
include_directories(SYSTEM "/usr/local/include/essentia/")
include_directories(SYSTEM "/usr/local/include/essentia/scheduler/")
include_directories(SYSTEM "/usr/local/include/essentia/streaming/")
include_directories(SYSTEM "/usr/local/include/essentia/utils")
include_directories(SYSTEM "/usr/include/taglib")
include_directories(SYSTEM "/usr/local/include/gaia2")
include_directories(SYSTEM "/usr/include/qt4")
include_directories(SYSTEM "/usr/include/qt4/QtCore")

# Compile jsoncpp from source
add_library(jsoncpp STATIC ${PROJECT_SOURCE_DIR}/../external/jsoncpp.cpp)

# Build the server executable
add_executable(server main.cpp WebsocketServer.cpp Analyzer.cpp)
target_link_libraries (server jsoncpp)
