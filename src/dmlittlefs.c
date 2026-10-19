/**
 * @file dmlittlefs.c
 * @brief dmlittlefs module: mount contexts (dmfsi _init/_deinit) and dmlittlefs_mkfs()
 *
 * The dmfsi file and directory operations live in dmlittlefs_file.c and
 * dmlittlefs_dir.c. This is the only translation unit that emits the DIF
 * signatures (ENABLE_DIF_REGISTRATIONS) and the module's own API
 * registrations (dmlittlefs.h).
 */
#define DMOD_ENABLE_REGISTRATION    ON
#define ENABLE_DIF_REGISTRATIONS    ON
#include "dmlittlefs_internal.h"
#include "dmlittlefs.h"
#include <string.h>

int dmod_init(const Dmod_Config_t* Config)
{
    (void)Config;
    Dmod_Printf("dmlittlefs: littlefs %u.%u (disk version %u.%u)\n", (unsigned)(LFS_VERSION >> 16),
                (unsigned)(LFS_VERSION & 0xFFFFu), (unsigned)(LFS_DISK_VERSION >> 16),
                (unsigned)(LFS_DISK_VERSION & 0xFFFFu));
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}

bool dmlittlefs_context_valid(dmfsi_context_t ctx)
{
    return ctx != NULL && ctx->magic == DMLITTLEFS_CONTEXT_MAGIC;
}

/* A context with an opened device and the littlefs configuration, not mounted. NULL (and @p device freed) on failure. */
static dmfsi_context_t context_create(char* device, const char* config, int* error)
{
    dmfsi_context_t ctx = Dmod_Malloc(sizeof(struct dmfsi_context));
    if (ctx == NULL)
    {
        Dmod_Free(device);
        *error = -ENOMEM;
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->device = device;
    ctx->mutex = Dmod_Mutex_New(true);
    *error = (ctx->mutex != NULL) ? dmlittlefs_block_attach(ctx, config) : -ENOMEM;
    if (*error != 0)
    {
        if (ctx->mutex != NULL)
        {
            Dmod_Mutex_Delete(ctx->mutex);
        }
        Dmod_Free(ctx->device);
        Dmod_Free(ctx);
        return NULL;
    }
    return ctx;
}

static void context_destroy(dmfsi_context_t ctx)
{
    if (ctx->mounted)
    {
        lfs_unmount(&ctx->lfs);
    }
    dmlittlefs_block_detach(ctx);
    Dmod_Mutex_Delete(ctx->mutex);
    ctx->magic = 0;
    Dmod_Free(ctx->device);
    Dmod_Free(ctx);
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, dmfsi_context_t, _init, (const char* config) )
{
    char* device = dmlittlefs_parse_device(config);
    if (device == NULL)
    {
        DMOD_LOG_ERROR("dmlittlefs needs a block device: \"device=/dev/<node>\" (got '%s')\n",
                       (config != NULL) ? config : "");
        return NULL;
    }
    int error = 0;
    dmfsi_context_t ctx = context_create(device, config, &error);
    if (ctx == NULL)
    {
        return NULL;
    }
    int ret = lfs_mount(&ctx->lfs, &ctx->cfg);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("No littlefs on '%s' (error %d) - create one with mklittlefs\n", ctx->device, ret);
        context_destroy(ctx);
        return NULL;
    }
    ctx->mounted = true;
    ctx->magic = DMLITTLEFS_CONTEXT_MAGIC;
    DMOD_LOG_INFO("Mounted littlefs from '%s' (%u blocks of %u bytes)\n", ctx->device,
                  (unsigned)ctx->cfg.block_count, (unsigned)ctx->cfg.block_size);
    return ctx;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _deinit, (dmfsi_context_t ctx) )
{
    if (!dmlittlefs_context_valid(ctx))
    {
        return DMFSI_ERR_INVALID;
    }
    context_destroy(ctx);
    return DMFSI_OK;
}

dmod_dmfsi_dif_api_declaration( 1.0, dmlittlefs, int, _context_is_valid, (dmfsi_context_t ctx) )
{
    return dmlittlefs_context_valid(ctx) ? 1 : 0;
}

dmod_dmlittlefs_api_declaration(1.0, int, _mkfs, ( const char* config ))
{
    char* device = dmlittlefs_parse_device(config);
    if (device == NULL)
    {
        return -EINVAL;
    }
    int error = 0;
    dmfsi_context_t ctx = context_create(device, config, &error);
    if (ctx == NULL)
    {
        return error;
    }
    int ret = lfs_format(&ctx->lfs, &ctx->cfg);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Formatting '%s' failed (littlefs error %d)\n", ctx->device, ret);
    }
    context_destroy(ctx);
    return dmlittlefs_to_errno(ret);
}
