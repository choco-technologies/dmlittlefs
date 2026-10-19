/**
 * @file dmlittlefs_util.c
 * @brief Error mapping, path and mount-configuration helpers of dmlittlefs
 */
#include "dmlittlefs_internal.h"
#include <string.h>

int dmlittlefs_to_dmfsi(int lfs_error)
{
    if (lfs_error >= 0)
    {
        return DMFSI_OK;
    }
    switch (lfs_error)
    {
        case LFS_ERR_NOENT:     return DMFSI_ERR_NOT_FOUND;
        case LFS_ERR_EXIST:     return DMFSI_ERR_EXISTS;
        case LFS_ERR_NOSPC:     return DMFSI_ERR_NO_SPACE;
        case LFS_ERR_NOTEMPTY:  return DMFSI_ERR_NOT_EMPTY;
        case LFS_ERR_INVAL:
        case LFS_ERR_NAMETOOLONG:
        case LFS_ERR_ISDIR:
        case LFS_ERR_NOTDIR:
        case LFS_ERR_BADF:      return DMFSI_ERR_INVALID;
        default:                return DMFSI_ERR_GENERAL;
    }
}

int dmlittlefs_to_errno(int lfs_error)
{
    switch (lfs_error)
    {
        case LFS_ERR_OK:        return 0;
        case LFS_ERR_NOENT:     return -ENOENT;
        case LFS_ERR_EXIST:     return -EEXIST;
        case LFS_ERR_NOSPC:     return -ENOSPC;
        case LFS_ERR_NOTEMPTY:  return -ENOTEMPTY;
        case LFS_ERR_NOMEM:     return -ENOMEM;
        case LFS_ERR_CORRUPT:   return -ENODEV;     /* no (valid) littlefs on the device */
        case LFS_ERR_INVAL:
        case LFS_ERR_BADF:
        case LFS_ERR_NAMETOOLONG:
        case LFS_ERR_ISDIR:
        case LFS_ERR_NOTDIR:    return -EINVAL;
        default:                return (lfs_error > 0) ? 0 : -EIO;
    }
}

bool dmlittlefs_is_root(const char* path)
{
    if (path == NULL)
    {
        return true;
    }
    while (*path == '/')
    {
        path++;
    }
    return *path == '\0';
}

char* dmlittlefs_make_path(const char* path)
{
    if (path == NULL || *path == '\0')
    {
        path = "/";
    }
    size_t length = strlen(path);
    char*  result = Dmod_Malloc(length + 2);
    if (result == NULL)
    {
        return NULL;
    }
    char* rest = result;
    if (path[0] != '/')
    {
        *rest++ = '/';
    }
    memcpy(rest, path, length + 1);
    return result;
}

/* ---- mount configuration: "device=<path>;cache=256;..." or just "<path>" ---- */

static bool is_separator(char c)
{
    return c == ';' || c == ',';
}

static bool key_matches(const char* key, size_t key_length, const char* expected)
{
    return strlen(expected) == key_length && strncmp(key, expected, key_length) == 0;
}


/* Heap copy of the value of the first entry whose key is @p key or @p alias (alias may be NULL). */
static char* entry_value(const char* config, const char* key, const char* alias)
{
    for (const char* entry = config; *entry != '\0'; )
    {
        size_t length = 0;
        while (entry[length] != '\0' && !is_separator(entry[length]))
        {
            length++;
        }
        size_t key_length = 0;
        while (key_length < length && entry[key_length] != '=')
        {
            key_length++;
        }
        if (key_length < length && (key_matches(entry, key_length, key) ||
                                    (alias != NULL && key_matches(entry, key_length, alias))))
        {
            return dmfsi_strndup(entry + key_length + 1, length - key_length - 1);
        }
        entry += length;
        entry += is_separator(*entry) ? 1 : 0;
    }
    return NULL;
}

char* dmlittlefs_parse_device(const char* config)
{
    if (config == NULL || *config == '\0')
    {
        return NULL;
    }
    if (strchr(config, '=') == NULL)
    {
        return Dmod_StrDup(config);
    }
    return entry_value(config, "device", "dev");
}

uint32_t dmlittlefs_config_uint(const char* config, const char* key, uint32_t default_value)
{
    char* text = (config != NULL) ? entry_value(config, key, NULL) : NULL;
    if (text == NULL)
    {
        return default_value;
    }
    uint32_t value = 0;
    bool     valid = text[0] != '\0';
    for (const char* c = text; *c != '\0' && valid; c++)
    {
        valid = *c >= '0' && *c <= '9' && value <= (UINT32_MAX - 9u) / 10u;
        value = valid ? value * 10u + (uint32_t)(*c - '0') : value;
    }
    Dmod_Free(text);
    return valid ? value : default_value;
}

/* Working directory of the caller on the heap, NULL on failure. */
static char* current_directory(void)
{
    for (size_t size = 64; size <= 4096; size *= 2)
    {
        char* buffer = Dmod_Malloc(size);
        if (buffer == NULL)
        {
            return NULL;
        }
        if (Dmod_GetCwd(buffer, size) != NULL)
        {
            return buffer;
        }
        Dmod_Free(buffer);
    }
    return NULL;
}

/* "<cwd>/<path>" for a relative path, a copy of @p path for an absolute one. */
static char* absolute_path(const char* path)
{
    if (path[0] == '/')
    {
        return Dmod_StrDup(path);
    }
    char* cwd = current_directory();
    if (cwd == NULL)
    {
        return NULL;
    }
    size_t cwd_length = strlen(cwd);
    char*  result     = Dmod_Malloc(cwd_length + strlen(path) + 2);
    if (result != NULL)
    {
        memcpy(result, cwd, cwd_length);
        result[cwd_length] = '/';
        strcpy(&result[cwd_length + 1], path);
    }
    Dmod_Free(cwd);
    return result;
}

/* Collapse "//", "/./" and "/<name>/../" of an absolute path, in place. */
static void normalize_path(char* path)
{
    char*       out = path;
    const char* in  = path;
    while (*in != '\0')
    {
        while (*in == '/')
        {
            in++;
        }
        size_t length = 0;
        while (in[length] != '\0' && in[length] != '/')
        {
            length++;
        }
        if (length == 2 && in[0] == '.' && in[1] == '.')
        {
            while (out > path && *--out != '/') {}
        }
        else if (length != 0 && !(length == 1 && in[0] == '.'))
        {
            *out++ = '/';
            memmove(out, in, length);
            out += length;
        }
        in += length;
    }
    if (out == path)
    {
        *out++ = '/';
    }
    *out = '\0';
}

char* dmlittlefs_canonical_path(const char* path)
{
    char* result = (path != NULL && *path != '\0') ? absolute_path(path) : NULL;
    if (result != NULL)
    {
        normalize_path(result);
    }
    return result;
}

/* True if @p rest is "p<digits>" - the name suffix dmdevfs gives partition nodes. */
static bool is_partition_suffix(const char* rest)
{
    if (rest[0] != 'p' || rest[1] == '\0')
    {
        return false;
    }
    for (rest++; *rest != '\0'; rest++)
    {
        if (*rest < '0' || *rest > '9')
        {
            return false;
        }
    }
    return true;
}

static bool is_partition_of(const char* partition, const char* device)
{
    size_t length = strlen(device);
    return strncmp(partition, device, length) == 0 && is_partition_suffix(&partition[length]);
}

bool dmlittlefs_paths_overlap(const char* a, const char* b)
{
    return strcmp(a, b) == 0 || is_partition_of(a, b) || is_partition_of(b, a);
}
