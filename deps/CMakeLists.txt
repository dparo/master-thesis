set(FETCHCONTENT_QUIET off CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED on CACHE BOOL "" FORCE)

## Note the difference between FetchContent `https://cmake.org/cmake/help/latest/module/FetchContent.html`
## and ExternalProject `https://cmake.org/cmake/help/latest/module/ExternalProject.html`
## is the following
##
# - FetchContent is only perfomed in **configure** steps, and it is not rerun at each build step
# - ExternalProject is performed at every **build** step.
#
## Note that if the external third party content is touched or modified, it will
## still be rebuilt regardless

set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/" CACHE STRING "(Must be a cache variable, do not change)")
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples

include(FetchContent)
include(ExternalProject)
find_package(Git REQUIRED)
find_program(MAKE_EXE NAMES gmake nmake make)


# Define custom Fetch function for submodules. If classical CMAKE FetchContent_Declare is used
# to download git submodules, it will without prompting just delete the submodule
# directory and start a new download from fresh each time Cmake configuration is perfomed.
# This behaviour is unwanted since, somebody may have modified the source
# code of a third party for testing reasons and cmake will happily delete it.
# This is especially true for people which have a workflow/habit of deleting the
# Cmake build directory. If the build directory is deleted, or a new build target
# is created (eg Release, Debug, RelWithDebugInfo)... Cmake will just delete the
# submodule directory contents and start a new clone... Very bad.
function(FetchGitSubmodule path)
    get_filename_component(name "${path}" NAME)

    fetchcontent_declare(
        ${name}
        DOWNLOAD_COMMAND ${GIT_EXECUTABLE} submodule update --recursive --init --depth 1 "${path}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}"
        SOURCE_DIR "${path}"
    )
    fetchcontent_makeavailable("${name}")
endfunction()

function(FetchArchiveConfigureAndMakeAndInstall name url)

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.local/third_party")
        file(REMOVE_RECURSE "${DEPS_DIR}/archives")
    endif()

    # In order to make find_package find the executables we need to run these download
    # steps at configure time (since find_package works at configure time).
    # Since `FetchContent_Declare` bans CONFIGURE_COMMAND, BUILD_COMMAND, INSTALL_COMMAND
    # compared to the the ExternalProject_Add
    # we are forced to combine all these commands inside the PATCH_COMMAND, which
    # doesn't make much sense since the patch command is meant to just apply patches, but
    # hey... whatever it works
    fetchcontent_declare(
        "${name}"
        URL "${url}"
        SOURCE_DIR "${DEPS_DIR}/archives/${name}"
        PATCH_COMMAND cd "${DEPS_DIR}/archives/${name}"
        && "${DEPS_DIR}/archives/${name}/configure" --prefix "${CMAKE_SOURCE_DIR}/.local/third_party"
        && "${MAKE_EXE}" -C "${DEPS_DIR}/archives/${name}" all
        && "${MAKE_EXE}" -C "${DEPS_DIR}/archives/${name}" install
    )
    if(NOT EXISTS "${DEPS_DIR}/archives/${name}")
        fetchcontent_makeavailable("${name}")
    endif()
endfunction()


fetchgitsubmodule("${DEPS_DIR}/log.c")
fetchgitsubmodule("${DEPS_DIR}/argparse")
fetchgitsubmodule("${DEPS_DIR}/Unity")


# Customize logc
add_library(logc STATIC "${CMAKE_SOURCE_DIR}/deps/log.c/src/log.c")
target_include_directories(logc PUBLIC "${CMAKE_SOURCE_DIR}/deps/log.c/src")

# Customize argparse
target_include_directories(argparse PUBLIC "${CMAKE_SOURCE_DIR}/deps/argparse")

# Customize unity
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_PRINT_FORMATTED "UNITY_EXCLUDE_SETJMP_H=1" "UNITY_USE_FLUSH_STDOUT=1")
if (MSVC)
    target_compile_definitions(unity PUBLIC "UNITY_NORETURN=__declspec(noreturn)")
endif()
