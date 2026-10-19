/**
 * @file dmlittlefs_dir.c
 * @brief dmfsi directory and path operations of dmlittlefs
 *
 * littlefs keeps no time stamps: the modification time is a user attribute
 * (DMLITTLEFS_ATTR_MTIME) that only _utime() sets; without it a time is 0.
 * There are no permissions either: _chmod() is accepted and ignored.
 */
#define DMOD_ENABLE_REGISTRATION    ON
#include "dmlittlefs_internal.h"
#include <string.h>

typedef struct
{
    uint32_t    magic;
    lfs_dir_t   dir;
    char*       path;       /**< Absolute path of the directory, to read the time of its entries */
} dir_handle_t;

static dir_handle_t* get_dir(dmfsi_context_t ctx, void* dp)
{
    dir_handle_t* dir = (dir_handle_t*)dp;
    if (!dmlittlefs_context_valid(ctx) || dir == NULL || dir->magic != DMLITTLEFS_DIR_MAGIC)
    {
        return NULL;
    }
    return dir;
}

static uint32_t read_mtime(dmfsi_context_t ctx, const char* path)
{
    uint32_t mtime = 0;
    lfs_ssize_t n = lfs_getattr(&ctx->lfs, path, DMLITTLEFS_ATTR_MTIME, &mtime, sizeof(mtime));
    return (n == (lfs_ssize_t)sizeof(mtime)) ? mtime : 0;
}

static uint32_t attr_of(const struct lfs_info* info)
{
    return (info->type == LFS_TYPE_DIR) ? DMFSI_ATTR_DIRECTORY : 0;
}

/* "<dir>/<name>" on the heap */
static char* child_path(const char* dir, const char* name)
{
    size_t dir_length = strlen(dir);
    char*  path = Dmod_Malloc(dir_length + strlen(name) + 2);
    if (path != NULL)
    {
        memcpy(path, dir, dir_length);
        size_t at = dir_length;
        if (at == 0 || path[at - 1] != '/')
        {
            path[at++] = '/';
        }
        strcpy(path + at, name);
    }
    return path;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _opendir, (dmfsi_context_t ctx, void** dp, const char* path) )
{
    if (!dmlittlefs_context_valid(ctx) || dp == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    dir_handle_t* dir = Dmod_Malloc(sizeof(dir_handle_t));
    if (dir == NULL)
    {
        return DMFSI_ERR_GENERAL;
    }
    memset(dir, 0, sizeof(*dir));
    dir->path = dmlittlefs_make_path(path);
    int ret = (dir->path != NULL) ? lfs_dir_open(&ctx->lfs, &dir->dir, dir->path) : LFS_ERR_NOMEM;
    if (ret < 0)
    {
        Dmod_Free(dir->path);
        Dmod_Free(dir);
        return dmlittlefs_to_dmfsi(ret);
    }
    dir->magic = DMLITTLEFS_DIR_MAGIC;
    *dp = dir;
    return DMFSI_OK;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _closedir, (dmfsi_context_t ctx, void* dp) )
{
    dir_handle_t* dir = get_dir(ctx, dp);
    if (dir == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    int ret = lfs_dir_close(&ctx->lfs, &dir->dir);
    dir->magic = 0;
    Dmod_Free(dir->path);
    Dmod_Free(dir);
    return dmlittlefs_to_dmfsi(ret);
}

dmod_dmfsi_dif_api_declaration( 2.0, dmlittlefs, int, _readdir, (dmfsi_context_t ctx, void* dp, dmfsi_dir_entry_t* entry) )
{
    dir_handle_t* dir = get_dir(ctx, dp);
    if (dir == NULL || entry == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    struct lfs_info info;
    int ret;
    do
    {
        ret = lfs_dir_read(&ctx->lfs, &dir->dir, &info);
    } while (ret > 0 && (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0));
    if (ret < 0)
    {
        return dmlittlefs_to_dmfsi(ret);
    }
    if (ret == 0)
    {
        return DMFSI_ERR_NOT_FOUND;
    }
    strncpy(entry->name, info.name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->size = (info.type == LFS_TYPE_REG) ? (dmfsi_size_t)info.size : 0;
    entry->attr = attr_of(&info);
    char* path = child_path(dir->path, info.name);
    entry->time = (path != NULL) ? read_mtime(ctx, path) : 0;
    Dmod_Free(path);
    return DMFSI_OK;
}

dmod_dmfsi_dif_api_declaration( 2.0, dmlittlefs, int, _stat, (dmfsi_context_t ctx, const char* path, dmfsi_stat_t* stat) )
{
    if (!dmlittlefs_context_valid(ctx) || stat == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    memset(stat, 0, sizeof(*stat));
    if (dmlittlefs_is_root(path))
    {
        stat->attr = DMFSI_ATTR_DIRECTORY;
        return DMFSI_OK;
    }
    char* lfs_path = dmlittlefs_make_path(path);
    if (lfs_path == NULL)
    {
        return DMFSI_ERR_GENERAL;
    }
    struct lfs_info info;
    int ret = lfs_stat(&ctx->lfs, lfs_path, &info);
    if (ret == 0)
    {
        stat->size = (info.type == LFS_TYPE_REG) ? (dmfsi_size_t)info.size : 0;
        stat->attr = attr_of(&info);
        stat->mtime = read_mtime(ctx, lfs_path);
        stat->ctime = stat->mtime;
        stat->atime = stat->mtime;
    }
    Dmod_Free(lfs_path);
    return dmlittlefs_to_dmfsi(ret);
}

/* Removes files and empty directories (dmvfs routes rmdir here as well). */
dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _unlink, (dmfsi_context_t ctx, const char* path) )
{
    if (!dmlittlefs_context_valid(ctx) || dmlittlefs_is_root(path))
    {
        return DMFSI_ERR_INVALID;
    }
    char* lfs_path = dmlittlefs_make_path(path);
    int ret = (lfs_path != NULL) ? lfs_remove(&ctx->lfs, lfs_path) : LFS_ERR_NOMEM;
    Dmod_Free(lfs_path);
    return dmlittlefs_to_dmfsi(ret);
}

/* A file at the destination is replaced atomically, like POSIX rename(). */
dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _rename, (dmfsi_context_t ctx, const char* oldpath, const char* newpath) )
{
    if (!dmlittlefs_context_valid(ctx) || dmlittlefs_is_root(oldpath) || dmlittlefs_is_root(newpath))
    {
        return DMFSI_ERR_INVALID;
    }
    char* from = dmlittlefs_make_path(oldpath);
    char* to = dmlittlefs_make_path(newpath);
    int ret = (from != NULL && to != NULL) ? lfs_rename(&ctx->lfs, from, to) : LFS_ERR_NOMEM;
    Dmod_Free(from);
    Dmod_Free(to);
    return dmlittlefs_to_dmfsi(ret);
}

/* littlefs has no permissions: accepted, ignored. */
dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _chmod, (dmfsi_context_t ctx, const char* path, int mode) )
{
    (void)path;
    (void)mode;
    return dmlittlefs_context_valid(ctx) ? DMFSI_OK : DMFSI_ERR_INVALID;
}

/* The modification time is a user attribute; the access time is not stored. */
dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _utime, (dmfsi_context_t ctx, const char* path, uint32_t atime, uint32_t mtime) )
{
    (void)atime;
    if (!dmlittlefs_context_valid(ctx) || dmlittlefs_is_root(path))
    {
        return DMFSI_ERR_INVALID;
    }
    char* lfs_path = dmlittlefs_make_path(path);
    int ret = (lfs_path != NULL) ? lfs_setattr(&ctx->lfs, lfs_path, DMLITTLEFS_ATTR_MTIME, &mtime, sizeof(mtime)) : LFS_ERR_NOMEM;
    Dmod_Free(lfs_path);
    return dmlittlefs_to_dmfsi(ret);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _mkdir, (dmfsi_context_t ctx, const char* path, int mode) )
{
    (void)mode;
    if (!dmlittlefs_context_valid(ctx) || dmlittlefs_is_root(path))
    {
        return DMFSI_ERR_INVALID;
    }
    char* lfs_path = dmlittlefs_make_path(path);
    int ret = (lfs_path != NULL) ? lfs_mkdir(&ctx->lfs, lfs_path) : LFS_ERR_NOMEM;
    Dmod_Free(lfs_path);
    return dmlittlefs_to_dmfsi(ret);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _direxists, (dmfsi_context_t ctx, const char* path) )
{
    if (!dmlittlefs_context_valid(ctx))
    {
        return DMFSI_ERR_INVALID;
    }
    if (dmlittlefs_is_root(path))
    {
        return 1;
    }
    char* lfs_path = dmlittlefs_make_path(path);
    struct lfs_info info;
    int ret = (lfs_path != NULL) ? lfs_stat(&ctx->lfs, lfs_path, &info) : LFS_ERR_NOMEM;
    Dmod_Free(lfs_path);
    return (ret == 0 && info.type == LFS_TYPE_DIR) ? 1 : 0;
}
