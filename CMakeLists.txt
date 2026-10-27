cmake_minimum_required(VERSION 3.20)
project(gbcemu)

set(CMAKE_CXX_STANDARD 20)

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Unsupported platform '${CMAKE_SYSTEM_NAME}', only Windows and Linux are supported")
endif()

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/x64)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/x64)

add_subdirectory(src)