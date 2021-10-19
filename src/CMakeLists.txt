
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(WARN_FLAGS "-Wall -Wextra -Wunused")
    string(APPEND CMAKE_C_FLAGS " ${WARN_FLAGS}")
    string(APPEND CMAKE_CXX_FLAGS " ${WARN_FLAGS}")
    string(APPEND CMAKE_LINKER_FLAGS " ${WARN_FLAGS}")


    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(NOWARN_FLAGS "-Wno-unused-parameter")
        string(APPEND CMAKE_C_FLAGS " ${NOWARN_FLAGS}")
        string(APPEND CMAKE_CXX_FLAGS " ${NOWARN_FLAGS}")
        string(APPEND CMAKE_LINKER_FLAGS " ${NOWARN_FLAGS}")
    endif()
endif()



add_library(libcptp
    types.c
    parser.c
    core.c
    validation.c
    solvers/stub.c
    solvers/mip.c
    )
target_include_directories(libcptp PUBLIC ./)
target_link_libraries(libcptp PUBLIC logc)

if (NOT WIN32)
    target_link_libraries(libcptp PUBLIC m)
endif()

if (CPLEX_FOUND)
    target_link_libraries(libcptp PUBLIC cplex-library)
    target_include_directories(libcptp PUBLIC "${CPLEX_INCLUDE_DIR}")
    target_compile_definitions(libcptp PUBLIC COMPILED_WITH_CPLEX=1)
endif()

add_executable(cptp main.c)
target_link_libraries(cptp PRIVATE libcptp argtable3)
