cmake_minimum_required(VERSION 3.14)

project(cptp
    VERSION 0.1.0
    DESCRIPTION "A Capacitated Profitable Tour Problem solver"
)

option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(BUILD_DOC "Build documentation" OFF)

if (BUILD_DOC)
    set(DOXYGEN_GENERATE_HTML ON)
    set(DOXYGEN_GENERATE_MAN OFF)
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/docs")
    set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_INLINE_SOURCES NO)
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_VERBATIM_HEADERS YES)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET "${CMAKE_SOURCE_DIR}/deps/doxygen-awesome-css/doxygen-awesome.css")
    find_package(Doxygen OPTIONAL_COMPONENTS mscgen dia dot)
endif()

if(NOT BUILD_DOC OR NOT DOXYGEN_FOUND)
    function(doxygen_add_docs)
    endfunction()
endif()



if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(ENABLE_ADRESS_SANITIZER "Enable address sanitizer for supported compilers and DEBUG builds" ON)
endif()



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


if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_ADRESS_SANITIZER)
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


if(NOT DEFINED CONTINOUS_INTEGRATION_ENABLED)
    set(CONTINOUS_INTEGRATION_ENABLED false)
endif()

if (CONTINOUS_INTEGRATION_ENABLED)
    add_definitions("-DCONTINOUS_INTEGRATION_ENABLED=1")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(CPLEX)

add_subdirectory(deps)
add_subdirectory(src)

include(CTest)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
