add_executable(rlist_persistent.test rlist_persistent.c)
target_link_libraries(rlist_persistent.test small small_unit)

create_test(rlist_persistent ${CMAKE_CURRENT_BINARY_DIR}/rlist_persistent.test)