cmake_minimum_required(VERSION 3.5)

project(routes C CXX ASM)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_INCLUDE_PATH})

include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckCCompilerFlag)
include(CheckSymbolExists)
include(CheckCSourceRuns)
include(CheckCXXSourceRuns)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(TestBigEndian)
include(CheckFunctionExists)
#include(FindOptionalPackage)
include(FindPackageMessage)
include(ExternalProject)
#include(CheckPatch)

#set(CMAKE_C_STANDARD 11)

include(cmake/utils.cmake)
include(cmake/compiler.cmake)
include(cmake/profile.cmake)
include(cmake/thread.cmake)
include(cmake/SetFiberStackSize.cmake)

check_symbol_exists(MAP_ANON sys/mman.h HAVE_MAP_ANON)
check_symbol_exists(MAP_ANONYMOUS sys/mman.h HAVE_MAP_ANONYMOUS)
check_symbol_exists(MADV_DONTNEED sys/mman.h HAVE_MADV_DONTNEED)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(cpuid.h HAVE_CPUID_H)
check_include_file(sys/prctl.h HAVE_PRCTL_H)

check_symbol_exists(O_DSYNC fcntl.h HAVE_O_DSYNC)
check_symbol_exists(fdatasync unistd.h HAVE_FDATASYNC)
check_symbol_exists(pthread_yield pthread.h HAVE_PTHREAD_YIELD)
check_symbol_exists(sched_yield sched.h HAVE_SCHED_YIELD)
check_symbol_exists(posix_fadvise fcntl.h HAVE_POSIX_FADVISE)
check_symbol_exists(fallocate fcntl.h HAVE_FALLOCATE)
check_symbol_exists(mremap sys/mman.h HAVE_MREMAP)

check_function_exists(sync_file_range HAVE_SYNC_FILE_RANGE)
check_function_exists(memmem HAVE_MEMMEM)
check_function_exists(memrchr HAVE_MEMRCHR)
check_function_exists(sendfile HAVE_SENDFILE)
if (HAVE_SENDFILE)
    if (TARGET_OS_LINUX)
        set(HAVE_SENDFILE_LINUX 1)
    else(HAVE_SENDFILE)
        set(HAVE_SENDFILE_BSD 1)
    endif()
endif()
check_function_exists(uuidgen HAVE_UUIDGEN)
set(CMAKE_REQUIRED_LIBRARIES "")
if (TARGET_OS_LINUX)
    set(CMAKE_REQUIRED_LIBRARIES rt)
endif ()
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME_DECL)
if (HAVE_CLOCK_GETTIME_DECL AND TARGET_OS_DARWIN)
    # ensure clock_gettime() is declared and actually available
    # in runtime (gh-1777)
    check_c_source_runs(
        "#include <time.h>\nint main() {struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return 0;}"
        HAVE_CLOCK_GETTIME)
else ()
    set(HAVE_CLOCK_GETTIME ${HAVE_CLOCK_GETTIME_DECL})
endif ()
set(CMAKE_REQUIRED_LIBRARIES "")
# According to `man 2 clockgettime` the glibc wrapper requires
# -lrt on glibc versions before 2.17. We need to check whether
# the function is available without -lrt to set this linker option
# conditionally.
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME_WITHOUT_RT)

check_symbol_exists(__get_cpuid cpuid.h HAVE_CPUID)
check_symbol_exists(strlcpy string.h HAVE_STRLCPY)
check_symbol_exists(strlcat string.h HAVE_STRLCAT)

# On Linux the GNU version of strerror_r is used, because we define
# the _GNU_SOURCE macro, see cmake/os.cmake.
check_c_source_compiles("
        #include <string.h>
        int main() { return strerror_r(0, NULL, 0)[0] == 0; }
    " HAVE_STRERROR_R_GNU)

# Checks for libev
include(CheckStructHasMember)
check_struct_has_member("struct stat" "st_mtim" "sys/stat.h"
    HAVE_STRUCT_STAT_ST_MTIM)
check_struct_has_member("struct stat" "st_mtimensec" "sys/stat.h"
    HAVE_STRUCT_STAT_ST_MTIMENSEC)

#
# Some versions of GNU libc define non-portable __libc_stack_end
# which we use to determine the end (or beginning, actually) of
# stack. Find whether or not it's present.
check_library_exists("" __libc_stack_end "" HAVE_LIBC_STACK_END)

check_function_exists(setproctitle HAVE_SETPROCTITLE)
check_function_exists(setprogname HAVE_SETPROGNAME)
check_function_exists(getprogname HAVE_GETPROGNAME)

check_symbol_exists(malloc_info malloc.h HAVE_MALLOC_INFO)

#
# LibCORO
#
include(BuildLibCORO)
libcoro_build()

#
# Specify where to look for include files.
#
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/lib)
include_directories(${PROJECT_SOURCE_DIR}/src/lib/small_extra/include)
include_directories(${PROJECT_BINARY_DIR}/src/lib/small_extra/small_extra/include)
include_directories(${PROJECT_SOURCE_DIR}/src/lib/small/include)
include_directories(${PROJECT_BINARY_DIR}/src/lib/small/small/include)
include_directories(${PROJECT_SOURCE_DIR}/src/lib/small/third_party)
include_directories(${PROJECT_SOURCE_DIR}/src/lib/core)
include_directories(${PROJECT_SOURCE_DIR}/third_party)

#
# Output compile-time defines into config.h. Do it at the end
# of the script to make sure all variables are set.
#
configure_file(
        "${PROJECT_SOURCE_DIR}/src/trivia/config.h.cmake"
        "${PROJECT_BINARY_DIR}/src/trivia/config.h"
)

enable_testing()

add_subdirectory(src)

set (sources
    src/c_penalty.c
    src/cli.c
    src/customer.c
    src/distance.c
    src/eama_solver.c
    src/ejection.c
    src/modification.c
    src/problem.c
    src/problem_decode.cc
    src/random_utils.c
    src/route.c
    src/solution.cc
    src/solution_encode.cc
    src/tw_penalty.c
    src/main.c
)

add_executable(routes ${sources})
#target_compile_options(routes PRIVATE -Wall -Wextra -Wpedantic -Wno-gnu-statement-expression)
target_link_libraries(routes small core)

add_subdirectory(test)
