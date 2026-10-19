# dmlittlefs Documentation

dmlittlefs is [littlefs](https://github.com/littlefs-project/littlefs) 2.10 for DMOD: a power-loss resilient
file system with dynamic wear levelling, made for NOR flash. It is mounted through dmvfs (it implements the
dmfsi DIF):

```
mklittlefs device=/dev/dmspiflashx/flash
mount -t dmlittlefs -o device=/dev/dmspiflashx/flash /mnt/flash
```

or from code:

```c
dmlittlefs_mkfs("device=/dev/dmspiflashx/flash");
dmvfs_mount_fs("dmlittlefs", "/mnt/flash", "device=/dev/dmspiflashx/flash");
```

## Contents

- **[api-reference.md](api-reference.md)** - mount options, behavior of the file operations, the module API

## Geometry (why it must match)

littlefs needs a block device with *read*, *program* and *erase*. dmlittlefs maps them to the dmdrvi block node:

| littlefs | dmlittlefs | for a NOR flash |
| --- | --- | --- |
| block | the erase block the device reports in `DMDRVI_IOCTL_BLOCK_GET_INFO` (`erase_block_size`) | 4096 B |
| program size | option `prog` (default 256) | the page of the chip |
| read size | option `read` (default 16) | |
| cache size | option `cache` (default 256) - RAM per open file, a multiple of read/prog, divides the block | |
| lookahead | option `lookahead` (default 32 bytes = 256 blocks window) | |
| block cycles | option `block_cycles` (default 500) - wear levelling: a metadata block is relocated after this many erases | |

A node that cannot erase (`DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED` missing) is refused. The geometry options must be
the same when the file system is mounted as when `mklittlefs` created it (block size and count must be equal;
`prog`/`read`/`cache` must be compatible).

## Behavior and limits

- **Power loss:** a write is atomic at the file close / `sync`; a loss in the middle leaves the previous version.
- **Space:** the usable space is less than the device: metadata is kept twice and littlefs wants free blocks to move
  data for wear levelling. A full volume gives `DMFSI_ERR_NO_SPACE`.
- **Names:** up to 255 bytes, any character but `/`. No hard or symbolic links.
- **Time:** littlefs has no time stamps. `utime()` stores the modification time in a user attribute (`'t'`, 32 bit
  seconds since 1970); a file that was never `utime()`d has time 0. Writing does not update it.
- **Permissions:** none - `chmod()` is accepted and ignored.
- **Rename:** replaces an existing file atomically, like POSIX `rename()`.
- **A part of a device:** `offset=<bytes>` and `size=<bytes>` (multiples of the erase block) select a region, so
  the rest of the flash can keep other data. `offset` is added to every address, `size` defaults to "to the end".
- One thread at a time works on a volume (every littlefs call takes the volume mutex); big reads and writes are
  made in 16 KiB chunks so that other threads get their turn.
- **Not for removable media or for FAT-style hosts:** to read the flash on a PC, dump it and use littlefs tools
  with the same geometry.

## Tools

`mklittlefs <config>` - creates an empty file system. The argument is the mount configuration (a device path, or
`device=<path>[,option=value...]`; entries are separated with `,` or `;` - use `,` in the shell, where `;` ends a command).

## Tests

`tests/dmlittlefs_test.c` formats and mounts an image file and goes through files, directories, rename, unlink,
big files, a full volume, time and remounting (`ctest`, or `dmod_loader build/dmf/test_dmlittlefs.dmf`).
On target: see the STM32F746G-DISCO session in the dmspiflash docs (`flashtest`) and mount the node as above.
