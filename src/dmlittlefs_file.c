/**
 * @file dmlittlefs_file.c
 * @brief dmfsi file operations of dmlittlefs
 */
#define DMOD_ENABLE_REGISTRATION    ON
#include "dmlittlefs_internal.h"
#include <string.h>

/*
 * littlefs locks the volume for the whole of a lfs_file_read() / lfs_file_write()
 * (LFS_THREADSAFE), so a big transfer keeps the other threads off the volume.
 * Transfers are made in chunks - between two of them the lock is free.
 */
#define CHUNK           16384u

static dmlittlefs_file_t* get_file(dmfsi_context_t ctx, void* fp)
{
    dmlittlefs_file_t* file = (dmlittlefs_file_t*)fp;
    if (!dmlittlefs_context_valid(ctx) || file == NULL || file->magic != DMLITTLEFS_FILE_MAGIC)
    {
        return NULL;
    }
    return file;
}

static int open_flags(int mode)
{
    int flags;
    switch (mode & DMFSI_O_RDWR)
    {
        case DMFSI_O_WRONLY:    flags = LFS_O_WRONLY;   break;
        case DMFSI_O_RDWR:      flags = LFS_O_RDWR;     break;
        default:                flags = LFS_O_RDONLY;   break;
    }
    flags |= (mode & DMFSI_O_CREAT) ? LFS_O_CREAT : 0;
    flags |= (mode & DMFSI_O_TRUNC) ? LFS_O_TRUNC : 0;
    flags |= (mode & DMFSI_O_APPEND) ? LFS_O_APPEND : 0;
    return flags;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _fopen, (dmfsi_context_t ctx, void** fp, const char* path, int mode, int attr) )
{
    (void)attr;
    if (!dmlittlefs_context_valid(ctx) || fp == NULL || path == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    char* lfs_path = dmlittlefs_make_path(path);
    dmlittlefs_file_t* file = Dmod_Malloc(sizeof(dmlittlefs_file_t));
    if (lfs_path == NULL || file == NULL)
    {
        Dmod_Free(lfs_path);
        Dmod_Free(file);
        return DMFSI_ERR_GENERAL;
    }
    memset(file, 0, sizeof(*file));
    int ret = lfs_file_open(&ctx->lfs, &file->file, lfs_path, open_flags(mode));
    Dmod_Free(lfs_path);
    if (ret < 0)
    {
        Dmod_Free(file);
        return dmlittlefs_to_dmfsi(ret);
    }
    file->magic = DMLITTLEFS_FILE_MAGIC;
    *fp = file;
    return DMFSI_OK;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _fclose, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    int ret = lfs_file_close(&ctx->lfs, &file->file);
    file->magic = 0;
    Dmod_Free(file);
    return dmlittlefs_to_dmfsi(ret);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _fread, (dmfsi_context_t ctx, void* fp, void* buffer, size_t size, size_t* read) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL || (buffer == NULL && size != 0))
    {
        return DMFSI_ERR_INVALID;
    }
    size_t total = 0;
    int    ret = 0;
    while (ret >= 0 && total < size)
    {
        lfs_size_t n = (size - total < CHUNK) ? (lfs_size_t)(size - total) : CHUNK;
        ret = (int)lfs_file_read(&ctx->lfs, &file->file, (uint8_t*)buffer + total, n);
        if (ret > 0)
        {
            total += (size_t)ret;
        }
        if (ret >= 0 && (lfs_size_t)ret < n)
        {
            break;      /* The end of the file */
        }
    }
    if (read != NULL)
    {
        *read = total;
    }
    file->error = (ret < 0) ? dmlittlefs_to_dmfsi(ret) : DMFSI_OK;
    return file->error;
}

static int write_file(dmlittlefs_file_t* file, dmfsi_context_t ctx, const void* buffer, size_t size, size_t* written)
{
    size_t total = 0;
    int    ret = 0;
    while (ret >= 0 && total < size)
    {
        lfs_size_t n = (size - total < CHUNK) ? (lfs_size_t)(size - total) : CHUNK;
        ret = (int)lfs_file_write(&ctx->lfs, &file->file, (const uint8_t*)buffer + total, n);
        if (ret > 0)
        {
            total += (size_t)ret;
        }
        if (ret >= 0 && (lfs_size_t)ret < n)
        {
            break;      /* The volume is full */
        }
    }
    if (written != NULL)
    {
        *written = total;
    }
    file->error = (ret < 0) ? dmlittlefs_to_dmfsi(ret) : DMFSI_OK;
    return file->error;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _fwrite, (dmfsi_context_t ctx, void* fp, const void* buffer, size_t size, size_t* written) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL || (buffer == NULL && size != 0))
    {
        return DMFSI_ERR_INVALID;
    }
    return write_file(file, ctx, buffer, size, written);
}

dmod_dmfsi_dif_api_declaration( 2.0, dmlittlefs, dmfsi_offset_t, _lseek, (dmfsi_context_t ctx, void* fp, dmfsi_offset_t offset, int whence) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    int lfs_whence;
    switch (whence)
    {
        case DMFSI_SEEK_SET:    lfs_whence = LFS_SEEK_SET;  break;
        case DMFSI_SEEK_CUR:    lfs_whence = LFS_SEEK_CUR;  break;
        case DMFSI_SEEK_END:    lfs_whence = LFS_SEEK_END;  break;
        default:                return DMFSI_ERR_INVALID;
    }
    lfs_soff_t position = lfs_file_seek(&ctx->lfs, &file->file, (lfs_soff_t)offset, lfs_whence);
    return (position >= 0) ? (dmfsi_offset_t)position : dmlittlefs_to_dmfsi((int)position);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _ioctl, (dmfsi_context_t ctx, void* fp, int request, void* arg) )
{
    (void)request;
    (void)arg;
    return (get_file(ctx, fp) == NULL) ? DMFSI_ERR_INVALID : DMFSI_ERR_GENERAL;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _sync, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    return (file == NULL) ? DMFSI_ERR_INVALID : dmlittlefs_to_dmfsi(lfs_file_sync(&ctx->lfs, &file->file));
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _fflush, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    return (file == NULL) ? DMFSI_ERR_INVALID : dmlittlefs_to_dmfsi(lfs_file_sync(&ctx->lfs, &file->file));
}

/* Next byte, -1 at the end of the file (or on error) */
dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _getc, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    unsigned char c = 0;
    return (lfs_file_read(&ctx->lfs, &file->file, &c, 1) == 1) ? (int)c : -1;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _putc, (dmfsi_context_t ctx, void* fp, int c) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    unsigned char byte = (unsigned char)c;
    size_t        written = 0;
    int result = write_file(file, ctx, &byte, 1, &written);
    if (result != DMFSI_OK)
    {
        return result;
    }
    return (written == 1) ? (int)byte : DMFSI_ERR_NO_SPACE;
}

dmod_dmfsi_dif_api_declaration( 2.0, dmlittlefs, dmfsi_offset_t, _tell, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    return (file == NULL) ? DMFSI_ERR_INVALID : (dmfsi_offset_t)lfs_file_tell(&ctx->lfs, &file->file);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _eof, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    if (file == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    return lfs_file_tell(&ctx->lfs, &file->file) >= lfs_file_size(&ctx->lfs, &file->file) ? 1 : 0;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, dmfsi_size_t, _size, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    return (file == NULL) ? 0 : (dmfsi_size_t)lfs_file_size(&ctx->lfs, &file->file);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _error, (dmfsi_context_t ctx, void* fp) )
{
    dmlittlefs_file_t* file = get_file(ctx, fp);
    return (file == NULL) ? DMFSI_ERR_INVALID : file->error;
}
