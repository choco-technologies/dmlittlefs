/**
 * @file dmlittlefs_internal.h
 * @brief Shared state and helpers of the dmlittlefs implementation
 *
 * Deliberately does not include dmlittlefs.h: the module's own API
 * registration objects are emitted by every translation unit that includes it
 * with DMOD_ENABLE_REGISTRATION defined, so only dmlittlefs.c includes it.
 */
#ifndef DMLITTLEFS_INTERNAL_H
#define DMLITTLEFS_INTERNAL_H

#include "dmod.h"
#include "dmfsi.h"
#include "lfs.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#define DMLITTLEFS_CONTEXT_MAGIC    0x4C465343u     /* 'LFSC' */
#define DMLITTLEFS_FILE_MAGIC       0x4C465346u     /* 'LFSF' */
#define DMLITTLEFS_DIR_MAGIC        0x4C465344u     /* 'LFSD' */

/* Defaults of the geometry parameters (mount options "cache", "lookahead", "block_cycles") */
#define DMLITTLEFS_PROG_SIZE        256u    /* Program unit = the page of a NOR flash */
#define DMLITTLEFS_READ_SIZE        16u
#define DMLITTLEFS_CACHE_SIZE       256u    /* Multiple of read/prog size, divides the block size */
#define DMLITTLEFS_LOOKAHEAD_SIZE   32u     /* Bytes (x8 blocks), multiple of 8 */
#define DMLITTLEFS_BLOCK_CYCLES     500u    /* Wear levelling: relocate a metadata block after N erases */

/** The attribute that stores the modification time (seconds since 1970, little endian 32 bit). */
#define DMLITTLEFS_ATTR_MTIME       0x74    /* 't' */

/** One mounted littlefs volume - the dmfsi context handed out by _init(). */
struct dmfsi_context
{
    uint32_t            magic;
    char*               device;         /**< Owned copy of the block device path */
    void*               device_file;    /**< The opened block device */
    void*               mutex;          /**< Serializes the littlefs calls (LFS_THREADSAFE) */
    uint64_t            offset;         /**< Start of the volume on the device, bytes (option "offset") */
    bool                has_erase;      /**< The device has DMDRVI_IOCTL_BLOCK_ERASE (an image file does not) */
    bool                mounted;
    struct lfs_config   cfg;
    lfs_t               lfs;
};

/** Open file - the fp handle of the dmfsi file operations. */
typedef struct
{
    uint32_t    magic;
    lfs_file_t  file;
    int         error;      /**< The last failure, DMFSI_OK if none */
} dmlittlefs_file_t;

/** Open directory - the dp handle of the dmfsi directory operations. */
typedef struct
{
    uint32_t    magic;
    lfs_dir_t   dir;
} dmlittlefs_dir_t;

/* ---- context (dmlittlefs.c) ---- */

bool dmlittlefs_context_valid(dmfsi_context_t ctx);

/* ---- block device (dmlittlefs_block.c) ---- */

/** Open @p device_path, read its geometry, fill ctx->cfg (callbacks, sizes). 0 or a negative errno. */
int  dmlittlefs_block_attach(dmfsi_context_t ctx, const char* config);

/** Close the device of @p ctx. */
void dmlittlefs_block_detach(dmfsi_context_t ctx);

/* ---- helpers (dmlittlefs_util.c) ---- */

int   dmlittlefs_to_dmfsi(int lfs_error);
int   dmlittlefs_to_errno(int lfs_error);

/** True for the root directory of a mount ("/", "" or NULL). */
bool  dmlittlefs_is_root(const char* path);

/** The path with a leading '/' (littlefs paths are absolute in the volume). Heap-owned. */
char* dmlittlefs_make_path(const char* path);

/** Block device path from the mount configuration ("device=<path>", "dev=<path>" or just "<path>"). Heap-owned. */
char* dmlittlefs_parse_device(const char* config);

/** Numeric mount option ("cache=512"), @p default_value when missing or invalid. */
uint32_t dmlittlefs_config_uint(const char* config, const char* key, uint32_t default_value);

/** Absolute form of a device path without "//", "." and ".." components. Heap-owned, NULL on failure. */
char* dmlittlefs_canonical_path(const char* path);

/** True if two canonical device paths can refer to the same sectors (the same node, or a device and its partition). */
bool  dmlittlefs_paths_overlap(const char* a, const char* b);

#endif // DMLITTLEFS_INTERNAL_H
