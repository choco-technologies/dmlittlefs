#ifndef DMLITTLEFS_H
#define DMLITTLEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmlittlefs_defs.h"

/**
 * dmlittlefs - littlefs, the power-loss resilient, wear levelling file system
 * for NOR flash, for DMOD.
 *
 * The file system itself is used through dmvfs - dmlittlefs implements the
 * dmfsi DIF, so it is mounted like any other file system:
 *
 *     dmvfs_mount_fs("dmlittlefs", "/mnt/flash", "device=/dev/dmspiflashx/flash");
 *
 * Mount options (separated by ';' or ','): "device=<path>" (required, or just the path),
 * "cache=<bytes>", "prog=<bytes>", "read=<bytes>", "lookahead=<bytes>",
 * "block_cycles=<erases>", "offset=<bytes>" and "size=<bytes>" (a part of the device, multiples of the
 * erase block) and "block_size=<bytes>" for an image file. See docs/README.md.
 *
 * The function below is the module's own API, for work outside of a mount.
 */

/**
 * Create a new, empty littlefs on a block device (or an image file).
 *
 * All data on the device is lost. The device must not be mounted. @p config
 * is the same string as the mount configuration (at least the device).
 *
 * @param config "device=<path>[;options]" or just the device path
 * @return 0 on success, negative errno on failure
 */
dmod_dmlittlefs_api(1.0, int, _mkfs, ( const char* config ));

#endif // DMLITTLEFS_H
