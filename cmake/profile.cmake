option(ENABLE_VALGRIND "Enable integration with valgrind, a memory analyzing tool" OFF)
if (ENABLE_VALGRIND)
    add_definitions(-UNVALGRIND)
else()
    add_definitions(-DNVALGRIND=1)
endif()

option(ENABLE_ASAN "Enable AddressSanitizer, a fast memory error detector based on compiler instrumentation" OFF)
if (ENABLE_ASAN)
    if (CMAKE_COMPILER_IS_GNUCC)
        message(FATAL_ERROR
            "\n"
            " Tarantool does not support GCC's AddressSanitizer. Use clang:\n"
            " $ git clean -xfd; git submodule foreach --recursive git clean -xfd\n"
            " $ CC=clang CXX=clang++ cmake . <...> -DENABLE_ASAN=ON && make -j\n"
            "\n")
    endif()

    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/asan/asan.supp")
    check_c_source_compiles("int main(void) {
        #include <sanitizer/asan_interface.h>
        void *x;
	    __sanitizer_finish_switch_fiber(x);
        return 0;
        }" ASAN_INTERFACE_OLD)
    check_c_source_compiles("int main(void) {
        #include <sanitizer/asan_interface.h>
        void *x;
	    __sanitizer_finish_switch_fiber(x, 0, 0);
        return 0;
    }" ASAN_INTERFACE_NEW)
    set(CMAKE_REQUIRED_FLAGS "")

    if (ASAN_INTERFACE_OLD)
         add_definitions(-DASAN_INTERFACE_OLD=1)
    elseif (ASAN_INTERFACE_NEW)
         add_definitions(-UASAN_INTERFACE_OLD)
    else()
        message(FATAL_ERROR "Cannot enable AddressSanitizer")
    endif()

    add_compile_flags("C;CXX" -fsanitize=address -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/asan/asan.supp)
endif()
