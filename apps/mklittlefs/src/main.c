#include "dmod.h"
#include "dmlittlefs.h"
#include <errno.h>
#include <string.h>

/*
 * mklittlefs <device> [options] - create an empty littlefs on <device> (all data on it is lost):
 *
 *   mklittlefs /dev/dmspiflashx/flash
 *   mklittlefs "device=/dev/dmspiflashx/flash;cache=512;block_cycles=200"
 *
 * The argument is the mount configuration of dmlittlefs (see docs/README.md): a device path or
 * "device=<path>" with options. The geometry options must be the same when mounting.
 */

static void print_usage(const char* prog)
{
    Dmod_Printf("Usage: %s <device | \"device=<path>[;option=value...]\">\n", prog);
    Dmod_Printf("\n");
    Dmod_Printf("Create an empty littlefs on a block device (ALL DATA ON IT IS LOST).\n");
    Dmod_Printf("Options (the same as when mounting): offset, size, cache, prog, read, lookahead, block_cycles.\n");
}

int main(int argc, char* argv[])
{
    if (argc != 2 || argv[1][0] == '-')
    {
        print_usage(argv[0]);
        return -EINVAL;
    }
    Dmod_Printf("Formatting %s...\n", argv[1]);
    int ret = dmlittlefs_mkfs(argv[1]);
    if (ret != 0)
    {
        Dmod_Printf("%s: cannot create the file system (%d)\n", argv[1], ret);
        return ret;
    }
    Dmod_Printf("%s: done\n", argv[1]);
    return 0;
}
