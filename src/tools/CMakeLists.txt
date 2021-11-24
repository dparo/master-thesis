add_subdirectory(perfprof)

add_executable(simplified-instance-converter
    simplified-instance-converter.c
    )
target_link_libraries(simplified-instance-converter PRIVATE libcptp argtable3::argtable3)
target_include_directories(simplified-instance-converter PRIVATE "${DEPS_DIR}/argtable3/src")
