
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


find_package(Git REQUIRED)

execute_process(COMMAND
  "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SHA1
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
)

# the date of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_DATE
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
)

# the subject of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%s
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
)

# generate version.c
configure_file("${CMAKE_SOURCE_DIR}/src/version.c.in" "${CMAKE_BINARY_DIR}/src/version.c" @ONLY)
doxygen_add_docs(docs
    "${CMAKE_SOURCE_DIR}/src/"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )


add_library(libcptp
    STATIC
    types.c
    parser.c
    parsing-utils.c
    core.c
    duality.c
    os.c
    validation.c
    visualization.c
    network.c

    # Stub solver
    solvers/stub/stub.c

    # MIP solver
    solvers/mip/mip.c
    $<$<BOOL:${CPLEX_FOUND}>:
        solvers/mip/warm-start.c
        solvers/mip/cuts/gsec.c
        solvers/mip/cuts/glm.c
        solvers/mip/cuts/rci.c
    >
)

target_include_directories(libcptp PUBLIC ./)
target_link_libraries(libcptp PUBLIC debugbreak logc libstb cjson-static libcrypto)

if (NOT WIN32)
    target_link_libraries(libcptp PUBLIC m)
endif()

if (CPLEX_FOUND)
    target_link_libraries(libcptp PUBLIC cplex-library)
    target_include_directories(libcptp PUBLIC "${CPLEX_INCLUDE_DIR}")
    target_compile_definitions(libcptp PUBLIC COMPILED_WITH_CPLEX=1)
endif()

add_executable(cptp
    "${CMAKE_BINARY_DIR}/src/version.c"
    main.c
    )
target_link_libraries(cptp PRIVATE libcptp argtable3::argtable3)
target_include_directories(cptp PRIVATE "${DEPS_DIR}/argtable3/src")


add_subdirectory(tools)
