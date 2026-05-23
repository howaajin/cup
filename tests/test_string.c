#include "core/allocator.h"
#include "core/string.h"
#include "cup/test.h"

#define ASSERT_STR_EQ(str, expected)                    \
    do                                                  \
    {                                                   \
        ASSERT(string_length(str) == strlen(expected)); \
        ASSERT(strcmp(str, expected) == 0);             \
        ASSERT(str[string_length(str)] == '\0');        \
    } while (0)

TEST(test_string_new, string)
{
    char* str = string_from_c_str(allocator_c(), "hello");
    char* cloned = string_new(allocator_c(), string_length(str), str);

    ASSERT(string_equal(str, cloned));
    ASSERT_STR_EQ(cloned, "hello");

    string_free(allocator_c(), str);
    string_free(allocator_c(), cloned);
}

TEST(test_string_from_c_str, string)
{
    char const test_str[] = "hello";
    char* str = string_from_c_str(allocator_c(), test_str);
    ASSERT_STR_EQ(str, "hello");
    string_free(allocator_c(), str);

    str = string_from_c_str(allocator_c(), NULL);
    ASSERT(str != NULL);
    ASSERT(string_length(str) == 0);
    ASSERT(str[0] == '\0');
    string_free(allocator_c(), str);

    str = string_from_c_str(allocator_c(), "");
    ASSERT(str != NULL);
    ASSERT(string_length(str) == 0);
    ASSERT(str[0] == '\0');
    string_free(allocator_c(), str);
}

TEST(test_string_shrink_to_fit, string)
{
    char* str = string_from_c_str(allocator_c(), "length7");
    ASSERT(string_length(str) == 7);

    string_printf(allocator_c(), str, "%s", "1234567890");
    size_t old_cap = array_capacity(str);
    array_size_lvalue(str) = 7;
    str[7] = '\0';

    str = string_shrink_to_fit(allocator_c(), str);
    size_t new_cap = array_capacity(str);
    ASSERT(new_cap <= old_cap);
    ASSERT_STR_EQ(str, "length7");

    string_free(allocator_c(), str);
}

TEST(test_string_push, string)
{
    char* str = NULL;
    string_putc(allocator_c(), str, 'a');
    string_putc(allocator_c(), str, 'b');
    string_putc(allocator_c(), str, 'c');
    ASSERT_STR_EQ(str, "abc");
    string_free(allocator_c(), str);
}

TEST(test_string_concat, string)
{
    char* str1 = string_from_c_str(allocator_c(), "abc");
    char* str2 = string_from_c_str(allocator_c(), "def");

    string_append(allocator_c(), str1, str2);
    ASSERT_STR_EQ(str1, "abcdef");

    char* empty = string_from_c_str(allocator_c(), "");
    string_append(allocator_c(), str1, empty);
    ASSERT_STR_EQ(str1, "abcdef");

    string_append(allocator_c(), str1, str1);
    ASSERT_STR_EQ(str1, "abcdefabcdef");

    string_free(allocator_c(), str1);
    string_free(allocator_c(), str2);
    string_free(allocator_c(), empty);
}

TEST(test_string_from_print, string)
{
    char* str = string_from_print(allocator_c(), "%s:%d", "file.c", 7);
    ASSERT_STR_EQ(str, "file.c:7");
    string_free(allocator_c(), str);
}

TEST(test_string_append_fmt, string)
{
    char const null_append_str[] = "test NULL append";
    char* null_append = string_append_slice(allocator_c(), NULL, sizeof(null_append_str) - 1, null_append_str);
    ASSERT_STR_EQ(null_append, null_append_str);

    char* str = string_from_c_str(allocator_c(), "str1");

    string_printf(allocator_c(), str, " %d ", 100);
    ASSERT_STR_EQ(str, "str1 100 ");

    string_printf(allocator_c(), str, "[%s]", str);
    ASSERT_STR_EQ(str, "str1 100 [str1 100 ]");

    char large_buf[513];
    memset(large_buf, 'A', 512);
    large_buf[512] = '\0';
    string_printf(allocator_c(), str, "%s", large_buf);
    ASSERT(string_length(str) == 20 + 512);
    ASSERT(str[string_length(str)] == '\0');

    string_free(allocator_c(), str);
}

TEST(test_string_equal, string)
{
    char* str1 = string_from_c_str(allocator_c(), "fooboo");
    char* str2 = string_from_c_str(allocator_c(), "fooboo1");
    char* str3 = string_from_c_str(allocator_c(), "fooboo");
    char* empty1 = string_from_c_str(allocator_c(), "");
    char* empty2 = string_from_c_str(allocator_c(), "");

    ASSERT(!string_equal(str1, str2));
    ASSERT(string_equal(str1, str3));
    ASSERT(string_equal(empty1, empty2));
    ASSERT(!string_equal(str1, empty1));

    string_free(allocator_c(), str1);
    string_free(allocator_c(), str2);
    string_free(allocator_c(), str3);
    string_free(allocator_c(), empty1);
    string_free(allocator_c(), empty2);
}

TEST(test_string_find_substr, string)
{
    char* str = string_from_c_str(allocator_c(), "test: substr ...");

    size_t pos = string_find_substr(str, 6, "substr");
    ASSERT(pos == 6);

    pos = string_find_substr(str, 3, "...");
    ASSERT(pos == 13);

    pos = string_find_substr(str, 4, "test");
    ASSERT(pos == 0);

    pos = string_find_substr(str, 4, "....");
    ASSERT(pos == string_npos);

    pos = string_find_substr(str, 100, "too long");
    ASSERT(pos == string_npos);

    pos = string_find_substr(str, 0, "not zero");
    ASSERT(pos == string_npos);

    char* empty = string_from_c_str(allocator_c(), "");
    pos = string_find_substr(empty, 1, "a");
    ASSERT(pos == string_npos);

    string_free(allocator_c(), str);
    string_free(allocator_c(), empty);
}

TEST(test_string_free, string)
{
    char* str = string_from_c_str(allocator_c(), "test_free");
    string_free(allocator_c(), str);
    string_free(allocator_c(), NULL);
}

TEST(test_string_clear, string)
{
    char* str = string_from_c_str(allocator_c(), "test_clear");
    string_clear(str);
    ASSERT(string_length(str) == 0);
    ASSERT(array_capacity(str) != 0);
}
