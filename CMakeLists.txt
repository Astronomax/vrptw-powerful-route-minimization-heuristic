cmake_minimum_required(VERSION 3.26)
project(routes C)

set(CMAKE_C_STANDARD 11)

include(cmake/utils.cmake)

#
# Specify where to look for include files.
#
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/lib)
include_directories(${PROJECT_SOURCE_DIR}/src/lib/small/include)
include_directories(${PROJECT_BINARY_DIR}/src/lib/small/small/include)
include_directories(${PROJECT_SOURCE_DIR}/src/lib/small/third_party)

enable_testing()

add_subdirectory(src)

set (sources
    src/c_penalty.c
    src/customer.c
    src/distance.c
    src/modification.c
    src/problem.c
    src/route.c
    src/solution.c
    src/tw_penalty.c
    src/main.c
)

add_executable(routes ${sources})
target_link_libraries(routes small)

add_subdirectory(test)