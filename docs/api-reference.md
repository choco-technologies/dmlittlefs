# dmlittlefs API Reference

dmlittlefs has two faces:

- the **dmfsi file system interface** (DIF) - how dmvfs uses it once it is mounted; applications use the regular
  file API (`Dmod_FileOpen()`, `Dmod_ReadDir()`, ...) on paths below the mount point,
- the **module API** in `dmlittlefs.h` - `dmlittlefs_mkfs()`, to create a file system before it can be mounted.

## Mount configuration string

Entries separated with `,` or `;`; unknown keys are ignored. A string without `=` is just the device path.

| Key | Default | Meaning |
|-----|---------|---------|
| `device=<path>` (also `dev=`) | required | Block device (dmdevfs node) or image file |
| `offset=<bytes>` | 0 | Start of the volume on the device, a multiple of the erase block |
| `size=<bytes>` | to the end | Size of the volume, a multiple of the erase block |
| `prog=<bytes>` | 256 | Program unit (the page of a NOR flash) |
| `read=<bytes>` | 16 | Read unit |
| `cache=<bytes>` | 256 | Cache of a file; a multiple of `read` and `prog`, divides the erase block |
| `lookahead=<bytes>` | 32 | Block allocator window (a multiple of 8) |
| `block_cycles=<n>` | 500 | Erases of a metadata block before it is relocated (wear levelling) |
| `block_size=<bytes>` | 4096 | Image files only: the block size (a block node reports its own) |

A device (or a device and its partition) can be used by one volume at a time; a second mount of it, or a
`dmlittlefs_mkfs()` of a mounted one, is refused with `-EBUSY`.

## Functions

```c
int dmlittlefs_mkfs(const char* config);
```

Create an empty littlefs on the device of `config` (the same string as for mounting). **All data of the volume is
lost.** The volume must not be mounted. Returns 0, or a negative errno (`-ENOENT` no such device, `-EBUSY` in use,
`-EINVAL` bad geometry, `-ENOTSUP` the node cannot erase, `-EIO`).

## File operations

The standard dmfsi set: `fopen` (`DMFSI_O_RDONLY/WRONLY/RDWR/CREAT/TRUNC/APPEND`), `fread`, `fwrite`, `lseek`,
`tell`, `eof`, `size`, `getc`, `putc`, `sync`/`fflush` (commit the file), `fclose`, `opendir`/`readdir`/`closedir`
(`.` and `..` are not listed), `stat`, `mkdir`, `unlink` (files and empty directories; a directory with entries gives
`DMFSI_ERR_NOT_EMPTY`), `rename`, `direxists`, `utime` (modification time attribute), `chmod` (ignored). `ioctl` is not
supported. Errors: `DMFSI_ERR_NOT_FOUND`, `_EXISTS`, `_NO_SPACE`, `_NOT_EMPTY`, `_INVALID`, `_GENERAL`.
