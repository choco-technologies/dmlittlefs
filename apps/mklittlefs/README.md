# mklittlefs

Command line front end of [dmlittlefs](../../README.md)'s `dmlittlefs_mkfs()`:
creates an empty littlefs on a block device (or an image file). **All data on the
device is lost.**

```
mklittlefs <device>
mklittlefs "device=<path>[,option=value...]"
```

The argument is the mount configuration of dmlittlefs (see
[docs/README.md](../../docs/README.md)): a device path, or `device=<path>` with
options. Separate options with `,` - a `;` ends a command in the shell. The geometry
options (`prog`, `read`, `cache`, `lookahead`, `block_cycles`, `offset`, `size`) have
to be the same when the file system is mounted.

```
> mklittlefs /dev/dmspiflashx/flash
Formatting /dev/dmspiflashx/flash...
/dev/dmspiflashx/flash: done
> mount -t dmlittlefs -o device=/dev/dmspiflashx/flash /ext
```

A part of a flash that also holds other data:

```
> mklittlefs device=/dev/dmspiflashx/flash,offset=15728640,size=1044480
```

The device must not be mounted (`-EBUSY`). On failure the negative errno is printed
and returned as the exit status (see `dmlittlefs_mkfs()` in
[docs/api-reference.md](../../docs/api-reference.md)).
