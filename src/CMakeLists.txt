
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set(WARN_FLAGS "-Wall -Wextra -Wunused")
  string(APPEND CMAKE_C_FLAGS " ${WARN_FLAGS}")
  string(APPEND CMAKE_CXX_FLAGS " ${WARN_FLAGS}")
  string(APPEND CMAKE_LINKER_FLAGS " ${WARN_FLAGS}")
endif()


find_package(CPLEX REQUIRED)

add_library(libcptp core.c parser.c)
target_include_directories(libcptp PUBLIC ./)
target_include_directories(libcptp PUBLIC "${CPLEX_INCLUDE_DIR}")
target_link_libraries(libcptp PUBLIC logc cplex-library)

add_executable(cptp main.c)
target_link_libraries(cptp PRIVATE libcptp argparse)
