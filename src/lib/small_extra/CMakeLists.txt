project(small_extra C CXX)
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

include(CheckFunctionExists)
include(CheckSymbolExists)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT DEFINED SMALL_EMBEDDED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Werror")
endif()

# Enable GNU glibc extentions.
add_definitions("-D_GNU_SOURCE")

set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")

message (STATUS "")

include_directories("${CMAKE_CURRENT_BINARY_DIR}/small_extra/include")
include_directories("${PROJECT_SOURCE_DIR}/include/small_extra")

set(lib_headers
    include/small_extra/util.h
    include/small_extra/rlist_persistent.h)

enable_testing()
add_subdirectory(test)

include(GNUInstallDirs)

install(FILES ${lib_headers} DESTINATION include/${PROJECT_NAME})
