add_compile_flags("C;CXX" "-Wno-unused" "-Wno-unused-result")
if(CC_HAS_WNO_TAUTOLOGICAL_COMPARE)
    add_compile_flags("C;CXX" "-Wno-tautological-compare")
endif()
file(GLOB all_sources *.c *.cc)
set_source_files_compile_flags(${all_sources})

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/box)

function(create_unit_test)
  cmake_parse_arguments(
    UNIT
    ""
    "PREFIX"
    "SOURCES;LIBRARIES;COMPILE_DEFINITIONS"
    ${ARGN}
  )
  message(STATUS "Creating unit test ${UNIT_PREFIX}.test")
  add_executable(${UNIT_PREFIX}.test ${UNIT_SOURCES})
  add_test(${UNIT_PREFIX} ${UNIT_PREFIX}.test)
  target_compile_definitions(${UNIT_PREFIX}.test PRIVATE ${UNIT_COMPILE_DEFINITIONS})
  target_link_libraries(${UNIT_PREFIX}.test ${UNIT_LIBRARIES})
  set(UNIT_TEST_TARGETS "${UNIT_TEST_TARGETS} ${UNIT_PREFIX}.test" PARENT_SCOPE)
endfunction()

add_library(unit STATIC unit.c)

set(common_sources
        ${PROJECT_SOURCE_DIR}/src/customer.c
        ${PROJECT_SOURCE_DIR}/src/distance.c
        ${PROJECT_SOURCE_DIR}/src/route.c
        ${PROJECT_SOURCE_DIR}/src/modification.c
        ${PROJECT_SOURCE_DIR}/src/problem.c
        ${PROJECT_SOURCE_DIR}/src/tw_penalty.c
        ${PROJECT_SOURCE_DIR}/src/c_penalty.c
        generators.c
)

create_unit_test(PREFIX tw_penalty
                 SOURCES tw_penalty.c ${common_sources}
                 LIBRARIES core unit
)

create_unit_test(PREFIX c_penalty
                 SOURCES c_penalty.c ${common_sources}
                 LIBRARIES core unit
)

create_unit_test(PREFIX distance
                 SOURCES distance.c ${common_sources}
                 LIBRARIES core unit
)

create_unit_test(PREFIX modification
        SOURCES modification.c ${common_sources}
        LIBRARIES core unit
)