include(cmake/utils.cmake)
include(CheckCXXCompilerFlag)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
# Remove VALGRIND code and assertions in *any* type of release build.
    add_definitions("-DNDEBUG" "-DNVALGRIND")
endif()

option(ENABLE_MARCH_NATIVE
    "Enable -march=native for release-like builds"
    ON)
option(ENABLE_LTO
    "Enable link-time optimization for release-like builds"
    ON)
option(ENABLE_FNO_PLT
    "Enable -fno-plt for release-like builds"
    ON)
option(ENABLE_OFAST
    "Enable -Ofast for release-like builds"
    ON)

set(PGO_MODE "OFF" CACHE STRING
    "PGO mode for release-like builds: OFF, GENERATE, USE")
set_property(CACHE PGO_MODE PROPERTY STRINGS OFF GENERATE USE)
set(PGO_PROFILE_DIR "${CMAKE_BINARY_DIR}/pgo-data" CACHE PATH
    "Directory used to store/read PGO profile data")

string(TOUPPER "${PGO_MODE}" PGO_MODE)
if (NOT PGO_MODE STREQUAL "OFF" AND
    NOT PGO_MODE STREQUAL "GENERATE" AND
    NOT PGO_MODE STREQUAL "USE")
    message(FATAL_ERROR
        "Unsupported PGO_MODE='${PGO_MODE}'. Expected OFF, GENERATE, or USE.")
endif()

#
# Perform build type specific configuration.
#
check_c_compiler_flag("-ggdb" CC_HAS_GGDB)
if (CC_HAS_GGDB)
    set (CC_DEBUG_OPT "-ggdb")
endif()

set (CMAKE_C_FLAGS_DEBUG
        "${CMAKE_C_FLAGS_DEBUG} ${CC_DEBUG_OPT} -O0 -fsanitize=undefined")
set (CMAKE_C_FLAGS_RELWITHDEBINFO
        "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CC_DEBUG_OPT} -O3")
set (CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} ${CC_DEBUG_OPT} -O0 -fsanitize=undefined")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO
        "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CC_DEBUG_OPT} -O3")

macro(append_release_compile_flags langs)
    foreach(_lang ${langs})
        string(REPLACE ";" " " _flags "${ARGN}")
        foreach(_config RELEASE RELWITHDEBINFO)
            set(CMAKE_${_lang}_FLAGS_${_config}
                "${CMAKE_${_lang}_FLAGS_${_config}} ${_flags}")
        endforeach()
        unset(_config)
        unset(_flags)
    endforeach()
    unset(_lang)
endmacro()

macro(append_release_linker_flags)
    string(REPLACE ";" " " _flags "${ARGN}")
    foreach(_kind EXE SHARED MODULE)
        foreach(_config RELEASE RELWITHDEBINFO)
            set(CMAKE_${_kind}_LINKER_FLAGS_${_config}
                "${CMAKE_${_kind}_LINKER_FLAGS_${_config}} ${_flags}")
        endforeach()
        unset(_config)
    endforeach()
    unset(_kind)
    unset(_flags)
endmacro()

set(RELEASE_TUNING_FLAGS "")
set(RELEASE_LINK_FLAGS "")

if (ENABLE_MARCH_NATIVE)
    check_c_compiler_flag("-march=native" CC_HAS_MARCH_NATIVE)
    check_cxx_compiler_flag("-march=native" CXX_HAS_MARCH_NATIVE)
    if (CC_HAS_MARCH_NATIVE AND CXX_HAS_MARCH_NATIVE)
        list(APPEND RELEASE_TUNING_FLAGS "-march=native")
    else()
        message(WARNING "ENABLE_MARCH_NATIVE=ON, but compiler does not support -march=native")
    endif()
endif()

if (ENABLE_FNO_PLT)
    check_c_compiler_flag("-fno-plt" CC_HAS_FNO_PLT)
    check_cxx_compiler_flag("-fno-plt" CXX_HAS_FNO_PLT)
    if (CC_HAS_FNO_PLT AND CXX_HAS_FNO_PLT)
        list(APPEND RELEASE_TUNING_FLAGS "-fno-plt")
    else()
        message(WARNING "ENABLE_FNO_PLT=ON, but compiler does not support -fno-plt")
    endif()
endif()

if (ENABLE_OFAST)
    check_c_compiler_flag("-Ofast" CC_HAS_OFAST)
    check_cxx_compiler_flag("-Ofast" CXX_HAS_OFAST)
    if (CC_HAS_OFAST AND CXX_HAS_OFAST)
        list(APPEND RELEASE_TUNING_FLAGS "-Ofast")
    else()
        message(WARNING "ENABLE_OFAST=ON, but compiler does not support -Ofast")
    endif()
endif()

if (ENABLE_LTO)
    check_c_compiler_flag("-flto" CC_HAS_FLTO)
    check_cxx_compiler_flag("-flto" CXX_HAS_FLTO)
    if (CC_HAS_FLTO AND CXX_HAS_FLTO)
        list(APPEND RELEASE_TUNING_FLAGS "-flto")
        list(APPEND RELEASE_LINK_FLAGS "-flto")
    else()
        message(WARNING "ENABLE_LTO=ON, but compiler does not support -flto")
    endif()
endif()

if (NOT PGO_MODE STREQUAL "OFF")
    if (CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        file(MAKE_DIRECTORY "${PGO_PROFILE_DIR}")
        set(PGO_PREFIX_FLAG "-fprofile-prefix-path=${CMAKE_BINARY_DIR}")
        if (PGO_MODE STREQUAL "GENERATE")
            list(APPEND RELEASE_TUNING_FLAGS
                "-fprofile-generate=${PGO_PROFILE_DIR}"
                "${PGO_PREFIX_FLAG}")
            list(APPEND RELEASE_LINK_FLAGS "-fprofile-generate=${PGO_PROFILE_DIR}")
        elseif(PGO_MODE STREQUAL "USE")
            list(APPEND RELEASE_TUNING_FLAGS
                "-fprofile-use=${PGO_PROFILE_DIR}"
                "-fprofile-correction"
                "${PGO_PREFIX_FLAG}")
            list(APPEND RELEASE_LINK_FLAGS
                "-fprofile-use=${PGO_PROFILE_DIR}"
                "-fprofile-correction")
        endif()
    else()
        message(FATAL_ERROR
            "PGO_MODE currently supports only GNU C/C++ compilers in this project.")
    endif()
endif()

if (RELEASE_TUNING_FLAGS)
    append_release_compile_flags("C;CXX" ${RELEASE_TUNING_FLAGS})
endif()

if (RELEASE_LINK_FLAGS)
    append_release_linker_flags(${RELEASE_LINK_FLAGS})
endif()

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    string_join(" " RELEASE_TUNING_FLAGS_STR ${RELEASE_TUNING_FLAGS})
    string_join(" " RELEASE_LINK_FLAGS_STR ${RELEASE_LINK_FLAGS})
    if (RELEASE_TUNING_FLAGS_STR)
        message(STATUS "Release compile tuning flags: ${RELEASE_TUNING_FLAGS_STR}")
    endif()
    if (RELEASE_LINK_FLAGS_STR)
        message(STATUS "Release link tuning flags: ${RELEASE_LINK_FLAGS_STR}")
    endif()
    if (NOT PGO_MODE STREQUAL "OFF")
        message(STATUS "PGO mode: ${PGO_MODE}")
        message(STATUS "PGO profile dir: ${PGO_PROFILE_DIR}")
    endif()
endif()

unset(CC_DEBUG_OPT)
