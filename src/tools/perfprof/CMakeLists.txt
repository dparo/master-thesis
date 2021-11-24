
add_executable(perfprof
    main.c
    )
target_link_libraries(perfprof PRIVATE libcptp argtable3::argtable3)
target_include_directories(perfprof PRIVATE "${DEPS_DIR}/argtable3/src")
