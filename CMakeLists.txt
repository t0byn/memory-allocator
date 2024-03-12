cmake_minimum_required(VERSION 3.0...3.27)

set(CMAKE_VERBOSE_MAKEFILE on)

project("memory_allocator")

list(APPEND HEADERS allocator.h)

list(APPEND SOURCES main.cc allocator.cc)

add_executable(memory_allocator ${SOURCES} ${HEADERS})

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()
