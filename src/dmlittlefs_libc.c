/**
 * @file dmlittlefs_libc.c
 * @brief C library functions littlefs needs and the DMOD module runtime may lack
 *
 * Modules are not linked against a C library - dmod provides a small set of
 * string functions (dmod/src/module/string.c), which has no memcmp(), strspn() and strcspn(). The
 * definitions are weak so that a dmod implementation takes precedence.
 */
#include <stddef.h>

__attribute__((weak)) int memcmp(const void* s1, const void* s2, size_t n)
{
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

/* Length of the initial segment of @p s made only of characters from @p accept (or none of them if !@p in) */
static size_t span(const char* s, const char* set, int in)
{
    size_t n = 0;
    for (; s[n] != '\0'; n++)
    {
        const char* c = set;
        while (*c != '\0' && *c != s[n])
        {
            c++;
        }
        if ((*c != '\0') != in)
        {
            break;
        }
    }
    return n;
}

__attribute__((weak)) size_t strspn(const char* s, const char* accept)
{
    return span(s, accept, 1);
}

__attribute__((weak)) size_t strcspn(const char* s, const char* reject)
{
    return span(s, reject, 0);
}
