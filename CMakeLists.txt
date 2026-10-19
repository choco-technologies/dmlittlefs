cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED DMOD_MODULE_VERSION)
    set(DMOD_MODULE_VERSION "0.1" CACHE STRING "DMOD module version")
endif()

# If a local dmod checkout is provided, use it instead of fetching from git
if(DEFINED DMOD_DIR AND NOT "${DMOD_DIR}" STREQUAL "")
    set(FETCHCONTENT_SOURCE_DIR_DMOD "${DMOD_DIR}" CACHE PATH "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    dmod
    GIT_REPOSITORY https://github.com/choco-technologies/dmod.git
    GIT_TAG        develop
)

set(DMOD_MODE "DMOD_MODULE" CACHE STRING "DMOD build mode")
set(DMOD_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(DMOD_BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(DMOD_BUILD_TOOLS OFF CACHE BOOL "Build tools")
set(DMOD_BUILD_TEMPLATES OFF CACHE BOOL "Build templates")

FetchContent_MakeAvailable(dmod)

project(dmlittlefs VERSION ${DMOD_MODULE_VERSION}
    DESCRIPTION "dmlittlefs DMOD library module"
    LANGUAGES C CXX)

set(DMOD_DIR ${dmod_SOURCE_DIR} CACHE PATH "DMOD source directory")
set(DMOD_SCRIPTS_DIR ${DMOD_DIR}/scripts CACHE PATH "DMOD scripts directory")
include(${DMOD_DIR}/paths.cmake)
dmod_setup_external_module()

# Name of the module
set(DMOD_MODULE_NAME        dmlittlefs)

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Path to the .dmr file - enables release package creation in build/packages/
set(DMOD_DMR_PATH           ${CMAKE_CURRENT_SOURCE_DIR}/dmlittlefs.dmr)

# List of MAL interfaces implemented by this module (leave empty if none)
set(DMOD_MAL_IMPLS          )

# List of DIF interfaces implemented by this module (leave empty if none)
set(DMOD_DIF_IMPLS          )

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    src/dmlittlefs.c
    src/dmlittlefs_file.c
    src/dmlittlefs_dir.c
    src/dmlittlefs_block.c
    src/dmlittlefs_util.c
    src/dmlittlefs_libc.c
    # littlefs 2.10.2 (upstream sources, configured by the definitions below)
    lib/littlefs/lfs.c
    lib/littlefs/lfs_util.c
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/littlefs
)

# littlefs on the DMOD runtime: see src/dmlittlefs_lfs_config.h (DMOD heap instead of malloc, the module
# locks the volume, no asserts / log output, portable bit helpers). lfs_util.h includes it through LFS_DEFINES.
target_compile_definitions(${DMOD_MODULE_NAME} PRIVATE
    LFS_DEFINES=dmlittlefs_lfs_config.h
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# dmfsi: the file system interface (DIF) implemented by dmlittlefs
# dmdrvi: DMDRVI_IOCTL_BLOCK_GET_INFO / _ERASE, the geometry and erase of dmdevfs block nodes
dmod_link_modules(${DMOD_MODULE_NAME}
    dmfsi
    dmdrvi@>=2.0
)

# mklittlefs - command line front end of dmlittlefs_mkfs()
add_subdirectory(apps/mklittlefs)
enable_testing()
add_subdirectory(tests)
