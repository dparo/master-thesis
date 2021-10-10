cmake_minimum_required(VERSION 3.14)

project(cptp VERSION 0.1.0)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_ADRESS_SANITIZER "Enable address sanitizer for supported compilers and DEBUG builds" ON)


set(CMAKE_C_STANDARD "11")
set(CMAKE_CXX_STANDARD "20")

if (false)
set(CXX_DISABLED_FEATURES_OPTS -fno-exceptions -fno-rtti)
# Directory level and below compile options for C++
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_DISABLED_FEATURES_OPTS}>")
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
# Force colored error output from the compiler when using Ninja
if(
  CMAKE_GENERATOR STREQUAL "Ninja" AND
  ((CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9) OR
  (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 3.5))
)
  # Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
  # Rationale in https://github.com/ninja-build/ninja/issues/814
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()


if(ENABLE_ADRESS_SANITIZER)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(SANITIZE_FLAGS "-fno-omit-frame-pointer -fsanitize=address")
    string(APPEND CMAKE_C_FLAGS_DEBUG " ${SANITIZE_FLAGS}")
    string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " ${SANITIZE_FLAGS}")

    string(APPEND CMAKE_CXX_FLAGS_DEBUG " ${SANITIZE_FLAGS}")
    string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " ${SANITIZE_FLAGS}")

    string(APPEND CMAKE_LINKER_FLAGS_DEBUG " ${SANITIZE_FLAGS}")
    string(APPEND CMAKE_LINKER_FLAGS_RELWITHDEBINFO " ${SANITIZE_FLAGS}")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_subdirectory(deps)
add_subdirectory(src)

include(CTest)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
