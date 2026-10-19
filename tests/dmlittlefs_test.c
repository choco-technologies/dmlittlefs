#define DMOD_ENABLE_REGISTRATION    ON
#define ENABLE_DIF_REGISTRATIONS    ON
#include "dmod_test.h"
#include "dmfsi.h"
#include "dmlittlefs.h"
#include <errno.h>
#include <string.h>

/*
 * dmlittlefs on an image file: every step formats a fresh image with
 * dmlittlefs_mkfs() and mounts it through the dmfsi DIF of the loaded module,
 * the same way dmvfs does on target (where the image is a dmdevfs block node).
 */

#define IMAGE_PATH      "dmlittlefs_test.img"
#define IMAGE_CONFIG    "device=" IMAGE_PATH ";block_size=4096"
#define IMAGE_SIZE      (1024u * 1024u)
#define BIG_FILE_SIZE   (200u * 1024u + 123u)

typedef struct
{
    dmod_dmfsi_init_t       init;
    dmod_dmfsi_deinit_t     deinit;
    dmod_dmfsi_fopen_t      fopen;
    dmod_dmfsi_fclose_t     fclose;
    dmod_dmfsi_fread_t      fread;
    dmod_dmfsi_fwrite_t     fwrite;
    dmod_dmfsi_lseek_t      lseek;
    dmod_dmfsi_tell_t       tell;
    dmod_dmfsi_eof_t        eof;
    dmod_dmfsi_size_t       size;
    dmod_dmfsi_opendir_t    opendir;
    dmod_dmfsi_readdir_t    readdir;
    dmod_dmfsi_closedir_t   closedir;
    dmod_dmfsi_stat_t       stat;
    dmod_dmfsi_unlink_t     unlink;
    dmod_dmfsi_rename_t     rename;
    dmod_dmfsi_utime_t      utime;
    dmod_dmfsi_mkdir_t      mkdir;
    dmod_dmfsi_direxists_t  direxists;
} lfs_dif_t;

static lfs_dif_t        g_fs;
static bool             g_loaded;
static dmfsi_context_t  g_ctx;

#define GET_DIF(module, name)   (g_fs.name = Dmod_GetDifFunction(module, dmod_dmfsi_##name##_sig)) != NULL

static bool load_module(void)
{
    if (Dmod_LoadModuleByName("dmlittlefs") == NULL || !Dmod_EnableModule("dmlittlefs", false, NULL))
    {
        return false;
    }
    Dmod_Context_t* m = Dmod_GetModuleContext("dmlittlefs");
    return GET_DIF(m, init) && GET_DIF(m, deinit) && GET_DIF(m, fopen) && GET_DIF(m, fclose) &&
           GET_DIF(m, fread) && GET_DIF(m, fwrite) && GET_DIF(m, lseek) && GET_DIF(m, tell) &&
           GET_DIF(m, eof) && GET_DIF(m, size) && GET_DIF(m, opendir) && GET_DIF(m, readdir) &&
           GET_DIF(m, closedir) && GET_DIF(m, stat) && GET_DIF(m, unlink) && GET_DIF(m, rename) &&
           GET_DIF(m, utime) && GET_DIF(m, mkdir) && GET_DIF(m, direxists);
}

/* An image of erased flash (0xFF), as a NOR flash is after a chip erase. */
static bool create_image(void)
{
    static uint8_t ones[4096];
    memset(ones, 0xFF, sizeof(ones));
    void* file = Dmod_FileOpen(IMAGE_PATH, "wb");
    if (file == NULL)
    {
        return false;
    }
    bool ok = true;
    for (uint32_t written = 0; ok && written < IMAGE_SIZE; written += sizeof(ones))
    {
        ok = Dmod_FileWrite(ones, 1, sizeof(ones), file) == sizeof(ones);
    }
    Dmod_FileClose(file);
    return ok;
}

void dmod_test_setup(void)
{
    g_loaded = g_loaded || load_module();
    g_ctx    = NULL;
    if (g_loaded && create_image() && dmlittlefs_mkfs(IMAGE_CONFIG) == 0)
    {
        g_ctx = g_fs.init(IMAGE_CONFIG);
    }
}

void dmod_test_teardown(void)
{
    if (g_ctx != NULL)
    {
        g_fs.deinit(g_ctx);
        g_ctx = NULL;
    }
    Dmod_FileRemove(IMAGE_PATH);
}

static bool write_text(const char* path, const char* text, int extra_mode)
{
    void*  fp      = NULL;
    size_t written = 0;
    if (g_fs.fopen(g_ctx, &fp, path, DMFSI_O_WRONLY | DMFSI_O_CREAT | extra_mode, 0) != DMFSI_OK)
    {
        return false;
    }
    int ret = g_fs.fwrite(g_ctx, fp, text, strlen(text), &written);
    return (g_fs.fclose(g_ctx, fp) == DMFSI_OK) && ret == DMFSI_OK && written == strlen(text);
}

static bool read_text(const char* path, char* buffer, size_t size)
{
    void*  fp   = NULL;
    size_t read = 0;
    if (g_fs.fopen(g_ctx, &fp, path, DMFSI_O_RDONLY, 0) != DMFSI_OK)
    {
        return false;
    }
    int ret = g_fs.fread(g_ctx, fp, buffer, size - 1, &read);
    buffer[read] = '\0';
    return (g_fs.fclose(g_ctx, fp) == DMFSI_OK) && ret == DMFSI_OK;
}

static uint8_t pattern_byte(uint32_t i)
{
    return (uint8_t)(i * 31u + (i >> 9) + 7u);
}

DMOD_TEST_STEP(mount_formatted_image)
{
    DMOD_TEST_EXPECT_TRUE(g_loaded);
    DMOD_TEST_EXPECT_NOT_NULL(g_ctx);
}

DMOD_TEST_STEP(unformatted_image_is_refused)
{
    g_fs.deinit(g_ctx);
    g_ctx = NULL;
    DMOD_TEST_EXPECT_TRUE(create_image());
    DMOD_TEST_EXPECT_NULL(g_fs.init(IMAGE_CONFIG));
}

DMOD_TEST_STEP(missing_device_is_refused)
{
    DMOD_TEST_EXPECT_NULL(g_fs.init(NULL));
    DMOD_TEST_EXPECT_NULL(g_fs.init("cache=256"));
    DMOD_TEST_EXPECT_NULL(g_fs.init("device=no_such_image.img"));
}

DMOD_TEST_STEP(invalid_geometry_is_refused)
{
    DMOD_TEST_EXPECT_NULL(g_fs.init(IMAGE_CONFIG ";cache=100"));   /* does not divide the block size */
}

DMOD_TEST_STEP(device_cannot_be_used_twice)
{
    DMOD_TEST_EXPECT_NULL(g_fs.init(IMAGE_CONFIG));
    DMOD_TEST_EXPECT_NE(dmlittlefs_mkfs(IMAGE_CONFIG), 0);
}

DMOD_TEST_STEP(write_and_read_back)
{
    char text[64];
    DMOD_TEST_EXPECT_TRUE(write_text("/hello.txt", "Hello, littlefs!", 0));
    DMOD_TEST_EXPECT_TRUE(read_text("/hello.txt", text, sizeof(text)));
    DMOD_TEST_EXPECT_EQ(strcmp(text, "Hello, littlefs!"), 0);
    dmfsi_stat_t st;
    DMOD_TEST_EXPECT_EQ(g_fs.stat(g_ctx, "/hello.txt", &st), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ((int)st.size, 16);
    DMOD_TEST_EXPECT_EQ((int)(st.attr & DMFSI_ATTR_DIRECTORY), 0);
}

DMOD_TEST_STEP(missing_file_and_exclusive_errors)
{
    void* fp = NULL;
    DMOD_TEST_EXPECT_EQ(g_fs.fopen(g_ctx, &fp, "/nope.txt", DMFSI_O_RDONLY, 0), DMFSI_ERR_NOT_FOUND);
    dmfsi_stat_t st;
    DMOD_TEST_EXPECT_EQ(g_fs.stat(g_ctx, "/nope.txt", &st), DMFSI_ERR_NOT_FOUND);
}

DMOD_TEST_STEP(append_seek_and_size)
{
    char text[64];
    DMOD_TEST_EXPECT_TRUE(write_text("/a.txt", "abc", 0));
    DMOD_TEST_EXPECT_TRUE(write_text("/a.txt", "def", DMFSI_O_APPEND));
    DMOD_TEST_EXPECT_TRUE(read_text("/a.txt", text, sizeof(text)));
    DMOD_TEST_EXPECT_EQ(strcmp(text, "abcdef"), 0);
    void* fp = NULL;
    DMOD_TEST_EXPECT_EQ(g_fs.fopen(g_ctx, &fp, "/a.txt", DMFSI_O_RDONLY, 0), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ((int)g_fs.size(g_ctx, fp), 6);
    DMOD_TEST_EXPECT_EQ((int)g_fs.lseek(g_ctx, fp, 2, DMFSI_SEEK_SET), 2);
    char c = 0;
    size_t n = 0;
    DMOD_TEST_EXPECT_EQ(g_fs.fread(g_ctx, fp, &c, 1, &n), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ((int)c, (int)'c');
    DMOD_TEST_EXPECT_EQ((int)g_fs.tell(g_ctx, fp), 3);
    DMOD_TEST_EXPECT_EQ((int)g_fs.lseek(g_ctx, fp, 0, DMFSI_SEEK_END), 6);
    DMOD_TEST_EXPECT_EQ(g_fs.eof(g_ctx, fp), 1);
    g_fs.fclose(g_ctx, fp);
    DMOD_TEST_EXPECT_TRUE(write_text("/a.txt", "xy", DMFSI_O_TRUNC));
    DMOD_TEST_EXPECT_TRUE(read_text("/a.txt", text, sizeof(text)));
    DMOD_TEST_EXPECT_EQ(strcmp(text, "xy"), 0);
}

DMOD_TEST_STEP(directories)
{
    char text[32];
    DMOD_TEST_EXPECT_EQ(g_fs.mkdir(g_ctx, "/dir", 0755), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ(g_fs.mkdir(g_ctx, "/dir", 0755), DMFSI_ERR_EXISTS);
    DMOD_TEST_EXPECT_EQ(g_fs.mkdir(g_ctx, "/dir/sub", 0755), DMFSI_OK);
    DMOD_TEST_EXPECT_TRUE(write_text("/dir/sub/f.txt", "nested", 0));
    DMOD_TEST_EXPECT_TRUE(read_text("/dir/sub/f.txt", text, sizeof(text)));
    DMOD_TEST_EXPECT_EQ(g_fs.direxists(g_ctx, "/dir/sub"), 1);
    DMOD_TEST_EXPECT_EQ(g_fs.direxists(g_ctx, "/dir/f"), 0);
    DMOD_TEST_EXPECT_EQ(g_fs.direxists(g_ctx, "/"), 1);

    void* dp = NULL;
    DMOD_TEST_EXPECT_EQ(g_fs.opendir(g_ctx, &dp, "/dir"), DMFSI_OK);
    dmfsi_dir_entry_t entry;
    DMOD_TEST_EXPECT_EQ(g_fs.readdir(g_ctx, dp, &entry), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ(strcmp(entry.name, "sub"), 0);
    DMOD_TEST_EXPECT_EQ((int)(entry.attr & DMFSI_ATTR_DIRECTORY), (int)DMFSI_ATTR_DIRECTORY);
    DMOD_TEST_EXPECT_EQ(g_fs.readdir(g_ctx, dp, &entry), DMFSI_ERR_NOT_FOUND);   /* "." and ".." are not listed */
    g_fs.closedir(g_ctx, dp);
}

DMOD_TEST_STEP(rename_replaces_a_file)
{
    char text[32];
    DMOD_TEST_EXPECT_TRUE(write_text("/one", "1", 0));
    DMOD_TEST_EXPECT_TRUE(write_text("/two", "2", 0));
    DMOD_TEST_EXPECT_EQ(g_fs.rename(g_ctx, "/one", "/two"), DMFSI_OK);
    DMOD_TEST_EXPECT_TRUE(read_text("/two", text, sizeof(text)));
    DMOD_TEST_EXPECT_EQ(strcmp(text, "1"), 0);
    dmfsi_stat_t st;
    DMOD_TEST_EXPECT_EQ(g_fs.stat(g_ctx, "/one", &st), DMFSI_ERR_NOT_FOUND);
}

DMOD_TEST_STEP(unlink_files_and_directories)
{
    DMOD_TEST_EXPECT_EQ(g_fs.mkdir(g_ctx, "/d", 0755), DMFSI_OK);
    DMOD_TEST_EXPECT_TRUE(write_text("/d/f", "x", 0));
    DMOD_TEST_EXPECT_EQ(g_fs.unlink(g_ctx, "/d"), DMFSI_ERR_NOT_EMPTY);
    DMOD_TEST_EXPECT_EQ(g_fs.unlink(g_ctx, "/d/f"), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ(g_fs.unlink(g_ctx, "/d"), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ(g_fs.unlink(g_ctx, "/d"), DMFSI_ERR_NOT_FOUND);
}

DMOD_TEST_STEP(big_file_round_trip)
{
    void*  fp = NULL;
    size_t n = 0;
    static uint8_t chunk[1000];
    DMOD_TEST_EXPECT_EQ(g_fs.fopen(g_ctx, &fp, "/big.bin", DMFSI_O_WRONLY | DMFSI_O_CREAT, 0), DMFSI_OK);
    for (uint32_t at = 0; at < BIG_FILE_SIZE; at += sizeof(chunk))
    {
        uint32_t len = (BIG_FILE_SIZE - at < sizeof(chunk)) ? BIG_FILE_SIZE - at : sizeof(chunk);
        for (uint32_t i = 0; i < len; i++)
            chunk[i] = pattern_byte(at + i);
        DMOD_TEST_EXPECT_EQ(g_fs.fwrite(g_ctx, fp, chunk, len, &n), DMFSI_OK);
        DMOD_TEST_EXPECT_EQ((int)n, (int)len);
    }
    DMOD_TEST_EXPECT_EQ(g_fs.fclose(g_ctx, fp), DMFSI_OK);

    DMOD_TEST_EXPECT_EQ(g_fs.fopen(g_ctx, &fp, "/big.bin", DMFSI_O_RDONLY, 0), DMFSI_OK);
    bool same = (g_fs.size(g_ctx, fp) == BIG_FILE_SIZE);
    for (uint32_t at = 0; same && at < BIG_FILE_SIZE; at += sizeof(chunk))
    {
        uint32_t len = (BIG_FILE_SIZE - at < sizeof(chunk)) ? BIG_FILE_SIZE - at : sizeof(chunk);
        same = g_fs.fread(g_ctx, fp, chunk, len, &n) == DMFSI_OK && n == len;
        for (uint32_t i = 0; same && i < len; i++)
            same = chunk[i] == pattern_byte(at + i);
    }
    g_fs.fclose(g_ctx, fp);
    DMOD_TEST_EXPECT_TRUE(same);
}

DMOD_TEST_STEP(remount_keeps_the_data)
{
    char text[32];
    DMOD_TEST_EXPECT_EQ(g_fs.mkdir(g_ctx, "/keep", 0755), DMFSI_OK);
    DMOD_TEST_EXPECT_TRUE(write_text("/keep/f.txt", "persistent", 0));
    DMOD_TEST_EXPECT_EQ(g_fs.deinit(g_ctx), DMFSI_OK);
    g_ctx = g_fs.init(IMAGE_CONFIG);
    DMOD_TEST_EXPECT_NOT_NULL(g_ctx);
    DMOD_TEST_EXPECT_TRUE(read_text("/keep/f.txt", text, sizeof(text)));
    DMOD_TEST_EXPECT_EQ(strcmp(text, "persistent"), 0);
}

DMOD_TEST_STEP(utime_sets_the_modification_time)
{
    DMOD_TEST_EXPECT_TRUE(write_text("/t.txt", "t", 0));
    dmfsi_stat_t st;
    DMOD_TEST_EXPECT_EQ(g_fs.stat(g_ctx, "/t.txt", &st), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ((int)st.mtime, 0);
    DMOD_TEST_EXPECT_EQ(g_fs.utime(g_ctx, "/t.txt", 0, 1700000000u), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ(g_fs.stat(g_ctx, "/t.txt", &st), DMFSI_OK);
    DMOD_TEST_EXPECT_EQ((int)st.mtime, 1700000000);
}

DMOD_TEST_STEP(a_full_volume_reports_no_space)
{
    void*  fp = NULL;
    size_t n = 0;
    static uint8_t block[4096];
    memset(block, 0x5A, sizeof(block));
    DMOD_TEST_EXPECT_EQ(g_fs.fopen(g_ctx, &fp, "/fill", DMFSI_O_WRONLY | DMFSI_O_CREAT, 0), DMFSI_OK);
    int ret = DMFSI_OK;
    for (uint32_t i = 0; ret == DMFSI_OK && i < 2u * IMAGE_SIZE / sizeof(block); i++)
        ret = g_fs.fwrite(g_ctx, fp, block, sizeof(block), &n);
    g_fs.fclose(g_ctx, fp);
    DMOD_TEST_EXPECT_EQ(ret, DMFSI_ERR_NO_SPACE);
    DMOD_TEST_EXPECT_EQ(g_fs.unlink(g_ctx, "/fill"), DMFSI_OK);
}
