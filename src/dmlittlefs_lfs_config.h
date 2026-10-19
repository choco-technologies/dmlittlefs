/**
 * @file dmlittlefs_lfs_config.h
 * @brief littlefs configuration for the DMOD runtime (included by lfs_util.h through LFS_DEFINES)
 */
#ifndef DMLITTLEFS_LFS_CONFIG_H
#define DMLITTLEFS_LFS_CONFIG_H

#include "dmod.h"

/* Allocations from the DMOD heap (there is no libc malloc in a module). LFS_NO_MALLOC is NOT
 * defined: it removes lfs_file_open() from the API; LFS_MALLOC takes precedence over malloc(). */
#define LFS_MALLOC(size)    Dmod_Malloc(size)
#define LFS_FREE(ptr)       Dmod_Free(ptr)

/* The volume is locked by the module through lfs_config.lock / unlock */
#define LFS_THREADSAFE

/* No asserts and no log output (no libc), the portable bit helpers (libgcc is not linked) */
#define LFS_NO_ASSERT
#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR
#define LFS_NO_INTRINSICS

#endif // DMLITTLEFS_LFS_CONFIG_H
