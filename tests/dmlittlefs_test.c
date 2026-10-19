#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmlittlefs.h"

static dmlittlefs_t g_handle = NULL;

void dmod_test_setup(void)
{
    g_handle = dmlittlefs_create();
}

void dmod_test_teardown(void)
{
    dmlittlefs_destroy(g_handle);
    g_handle = NULL;
}

DMOD_TEST_STEP(dmlittlefs_create)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_handle);
}

DMOD_TEST_STEP(dmlittlefs_is_valid)
{
    DMOD_TEST_EXPECT_TRUE(dmlittlefs_is_valid(g_handle));
}

DMOD_TEST_STEP(dmlittlefs_destroy_null)
{
    /* Destroying NULL must not crash. */
    dmlittlefs_destroy(NULL);
}
