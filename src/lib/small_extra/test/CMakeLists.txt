add_executable(rlist_persistent.test rlist_persistent.c)
target_link_libraries(rlist_persistent.test small small_unit)

include_directories("${PROJECT_SOURCE_DIR}/include")

create_test(rlist_persistent ${CMAKE_CURRENT_BINARY_DIR}/rlist_persistent.test)

if(POLICY CMP0037)
    cmake_policy(SET CMP0037 OLD) # don't blame "test" target name
endif(POLICY CMP0037)

set(small_extra_tests rlist_persistent.test)

#add_dependencies(test ${small_extra_tests})
