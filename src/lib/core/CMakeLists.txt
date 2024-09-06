set(core_sources
    diag.c
    say.c
    memory.c
    fiber.c
    exception.cc
    reflection.c
    random.c
    tt_strerror.c
    cord_on_demand.cc
)

add_library(core STATIC ${core_sources})

target_link_libraries(core small bit ${LIBCORO_LIBRARIES})
