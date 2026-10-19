/**
 * @file dmlittlefs_block.c
 * @brief littlefs block device layer on top of the DMOD file API
 *
 * The volume lives on a dmdevfs block node (e.g. /dev/dmspiflashx/flash): reads and
 * programs are file reads/writes at byte offsets, an erase is
 * DMDRVI_IOCTL_BLOCK_ERASE of the erase block. On the host an image file works too
 * (no erase ioctl: an erase writes 0xFF). The littlefs block is the erase block of
 * the device; the program unit defaults to the 256 byte page of a NOR flash.
 */
#include "dmlittlefs_internal.h"
#include "dmdrvi_ioctl.h"
#include <string.h>

#define MAX_VOLUMES     4u
#define IMAGE_BLOCK     4096u       /* Block size of an image file (option "block_size") */

static char* g_attached[MAX_VOLUMES];   /* Canonical device paths in use, guarded by the dmod loader's single mount path */

static int seek_to(dmfsi_context_t ctx, lfs_block_t block, lfs_off_t off)
{
    uint64_t position = ctx->offset + (uint64_t)block * ctx->cfg.block_size + off;
    return Dmod_FileSeek(ctx->device_file, (Dmod_FileOffset_t)position, DMFSI_SEEK_SET) == 0 ? 0 : LFS_ERR_IO;
}

static int block_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size)
{
    dmfsi_context_t ctx = (dmfsi_context_t)c->context;
    if (seek_to(ctx, block, off) != 0)
    {
        return LFS_ERR_IO;
    }
    return Dmod_FileRead(buffer, 1, size, ctx->device_file) == size ? 0 : LFS_ERR_IO;
}

static int block_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size)
{
    dmfsi_context_t ctx = (dmfsi_context_t)c->context;
    if (seek_to(ctx, block, off) != 0)
    {
        return LFS_ERR_IO;
    }
    return Dmod_FileWrite(buffer, 1, size, ctx->device_file) == size ? 0 : LFS_ERR_IO;
}

/* An image file has no erase: a block of 0xFF is what an erased flash reads as. */
static int erase_by_write(dmfsi_context_t ctx, lfs_block_t block)
{
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof(ones));
    for (lfs_off_t off = 0; off < ctx->cfg.block_size; off += sizeof(ones))
    {
        int ret = block_prog(&ctx->cfg, block, off, ones, sizeof(ones));
        if (ret != 0)
        {
            return ret;
        }
    }
    return 0;
}

static int block_erase(const struct lfs_config* c, lfs_block_t block)
{
    dmfsi_context_t ctx = (dmfsi_context_t)c->context;
    if (!ctx->has_erase)
    {
        return erase_by_write(ctx, block);
    }
    dmdrvi_block_range_t range = { .offset = (dmdrvi_offset_t)(ctx->offset + (uint64_t)block * c->block_size), .length = c->block_size };
    return Dmod_Ioctl(ctx->device_file, DMDRVI_IOCTL_BLOCK_ERASE, &range) == 0 ? 0 : LFS_ERR_IO;
}

static int block_sync(const struct lfs_config* c)
{
    (void)c;        /* Every write of the device finishes before it returns */
    return 0;
}

static int block_lock(const struct lfs_config* c)
{
    dmfsi_context_t ctx = (dmfsi_context_t)c->context;
    return Dmod_Mutex_Lock(ctx->mutex) == 0 ? 0 : LFS_ERR_IO;
}

static int block_unlock(const struct lfs_config* c)
{
    dmfsi_context_t ctx = (dmfsi_context_t)c->context;
    Dmod_Mutex_Unlock(ctx->mutex);
    return 0;
}

/* Block size and count from the device: the erase block of a block node, the file size for an image.
 * Options "offset" and "size" (bytes) select a part of the device - a partition of a flash that also holds
 * other data; both are multiples of the block size. */
static int read_geometry(dmfsi_context_t ctx, const char* config)
{
    uint64_t device_size;
    dmdrvi_block_info_t info = { 0 };
    if (Dmod_Ioctl(ctx->device_file, DMDRVI_IOCTL_BLOCK_GET_INFO, &info) == 0 && info.block_count != 0)
    {
        if ((info.flags & DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED) == 0 || info.erase_block_size == 0)
        {
            DMOD_LOG_ERROR("'%s' cannot erase blocks - littlefs needs a flash-like block device\n", ctx->device);
            return -ENOTSUP;
        }
        ctx->has_erase = true;
        ctx->cfg.block_size = info.erase_block_size;
        device_size = (uint64_t)info.block_count * info.logical_block_size;
    }
    else
    {
        ctx->cfg.block_size = dmlittlefs_config_uint(config, "block_size", IMAGE_BLOCK);
        device_size = (uint64_t)Dmod_FileSize(ctx->device_file);
    }
    ctx->offset = dmlittlefs_config_uint(config, "offset", 0);
    if (ctx->cfg.block_size == 0 || ctx->offset % ctx->cfg.block_size != 0 || ctx->offset >= device_size)
    {
        return -EINVAL;
    }
    uint64_t size = dmlittlefs_config_uint(config, "size", 0);
    size = (size != 0) ? size : device_size - ctx->offset;
    if (ctx->offset + size > device_size)
    {
        return -EINVAL;
    }
    ctx->cfg.block_count = (lfs_size_t)(size / ctx->cfg.block_size);
    return ctx->cfg.block_count != 0 ? 0 : -EIO;
}

/* littlefs asserts on these (the assertions are compiled out): check them here. */
static bool geometry_valid(const struct lfs_config* c)
{
    return c->read_size != 0 && c->prog_size != 0 && c->cache_size != 0 && c->block_size != 0 && c->block_count != 0 &&
           c->cache_size % c->read_size == 0 && c->cache_size % c->prog_size == 0 &&
           c->block_size % c->cache_size == 0 && c->block_size % c->prog_size == 0 &&
           c->block_size % c->read_size == 0 &&
           c->lookahead_size != 0 && c->lookahead_size % 8u == 0;
}

static void fill_config(dmfsi_context_t ctx, const char* config)
{
    struct lfs_config* c = &ctx->cfg;
    c->context = ctx;
    c->read = block_read;
    c->prog = block_prog;
    c->erase = block_erase;
    c->sync = block_sync;
    c->lock = block_lock;
    c->unlock = block_unlock;
    c->read_size = dmlittlefs_config_uint(config, "read", DMLITTLEFS_READ_SIZE);
    c->prog_size = dmlittlefs_config_uint(config, "prog", DMLITTLEFS_PROG_SIZE);
    c->cache_size = dmlittlefs_config_uint(config, "cache", DMLITTLEFS_CACHE_SIZE);
    c->lookahead_size = dmlittlefs_config_uint(config, "lookahead", DMLITTLEFS_LOOKAHEAD_SIZE);
    c->block_cycles = (int32_t)dmlittlefs_config_uint(config, "block_cycles", DMLITTLEFS_BLOCK_CYCLES);
}

/* Reserve @p path: a device (or a device and its partition) is used by one volume at a time. */
static int reserve_device(const char* path, char** slot_out)
{
    char* canonical = dmlittlefs_canonical_path(path);
    if (canonical == NULL)
    {
        return -ENOMEM;
    }
    unsigned free_slot = MAX_VOLUMES;
    for (unsigned i = 0; i < MAX_VOLUMES; i++)
    {
        if (g_attached[i] == NULL)
        {
            free_slot = (free_slot == MAX_VOLUMES) ? i : free_slot;
        }
        else if (dmlittlefs_paths_overlap(g_attached[i], canonical))
        {
            DMOD_LOG_ERROR("'%s' overlaps '%s', which is already mounted or being formatted\n", path, g_attached[i]);
            Dmod_Free(canonical);
            return -EBUSY;
        }
    }
    if (free_slot == MAX_VOLUMES)
    {
        Dmod_Free(canonical);
        return -EBUSY;
    }
    g_attached[free_slot] = canonical;
    *slot_out = canonical;
    return 0;
}

static void release_device(const char* canonical)
{
    for (unsigned i = 0; i < MAX_VOLUMES; i++)
    {
        if (g_attached[i] != NULL && g_attached[i] == canonical)
        {
            Dmod_Free(g_attached[i]);
            g_attached[i] = NULL;
        }
    }
}

int dmlittlefs_block_attach(dmfsi_context_t ctx, const char* config)
{
    char* reserved = NULL;
    int   ret = reserve_device(ctx->device, &reserved);
    if (ret != 0)
    {
        return ret;
    }
    ctx->device_file = Dmod_FileOpen(ctx->device, "r+b");
    if (ctx->device_file == NULL)
    {
        DMOD_LOG_ERROR("Cannot open block device '%s'\n", ctx->device);
        release_device(reserved);
        return -ENOENT;
    }
    fill_config(ctx, config);
    ret = read_geometry(ctx, config);
    if (ret == 0 && !geometry_valid(&ctx->cfg))
    {
        DMOD_LOG_ERROR("Invalid geometry on '%s': block %u x %u, read %u, prog %u, cache %u, lookahead %u\n", ctx->device,
                       (unsigned)ctx->cfg.block_size, (unsigned)ctx->cfg.block_count, (unsigned)ctx->cfg.read_size,
                       (unsigned)ctx->cfg.prog_size, (unsigned)ctx->cfg.cache_size, (unsigned)ctx->cfg.lookahead_size);
        ret = -EINVAL;
    }
    if (ret != 0)
    {
        Dmod_FileClose(ctx->device_file);
        ctx->device_file = NULL;
        release_device(reserved);
    }
    return ret;
}

void dmlittlefs_block_detach(dmfsi_context_t ctx)
{
    if (ctx->device_file == NULL)
    {
        return;
    }
    Dmod_FileClose(ctx->device_file);
    ctx->device_file = NULL;
    char* canonical = dmlittlefs_canonical_path(ctx->device);
    if (canonical != NULL)
    {
        for (unsigned i = 0; i < MAX_VOLUMES; i++)
        {
            if (g_attached[i] != NULL && strcmp(g_attached[i], canonical) == 0)
            {
                Dmod_Free(g_attached[i]);
                g_attached[i] = NULL;
            }
        }
        Dmod_Free(canonical);
    }
}
