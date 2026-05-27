#include "core/allocator.h"
#include "core/path.h"
#include "core/string.h"
#include "cup/test.h"

TEST(test_path_is_empty, path)
{
    ASSERT(path_is_empty(""));
    ASSERT(!path_is_empty("foo"));
    ASSERT(!path_is_empty("."));
}

TEST(test_path_is_absolute, path)
{
    ASSERT(path_is_absolute("/foo"));
    ASSERT(path_is_absolute("C:/foo"));
    ASSERT(path_is_absolute("C:"));
    ASSERT(!path_is_absolute("foo"));
    ASSERT(!path_is_absolute("foo/bar"));
    ASSERT(!path_is_absolute("."));
}

TEST(test_path_root_name, path)
{
    char* r;
    r = path_root_name("C:/foo", allocator_temp());
    ASSERT(string_equal(r, "C:"));

    r = path_root_name("C:", allocator_temp());
    ASSERT(string_equal(r, "C:"));

    r = path_root_name("/foo", allocator_temp());
    ASSERT(string_equal(r, ""));

    r = path_root_name("foo", allocator_temp());
    ASSERT(string_equal(r, ""));
}

TEST(test_path_root_directory, path)
{
    char* r;

    r = path_root_directory("/foo", allocator_temp());
    ASSERT(string_equal(r, "/"));

    r = path_root_directory("C:/foo", allocator_temp());
    ASSERT(string_equal(r, "/"));

    r = path_root_directory("C:", allocator_temp());
    ASSERT(string_equal(r, ""));

    r = path_root_directory("foo", allocator_temp());
    ASSERT(string_equal(r, ""));
}

TEST(test_path_root_path, path)
{
    char* r;

    r = path_root_path("C:/foo", allocator_temp());
    ASSERT(string_equal(r, "C:/"));

    r = path_root_path("/foo", allocator_temp());
    ASSERT(string_equal(r, "/"));

    r = path_root_path("C:", allocator_temp());
    ASSERT(string_equal(r, "C:"));

    r = path_root_path("foo", allocator_temp());
    ASSERT(string_equal(r, ""));
}

TEST(test_path_relative_path, path)
{
    ASSERT(string_equal(path_relative_path("/foo/bar"), "foo/bar"));
    ASSERT(string_equal(path_relative_path("C:/foo/bar"), "foo/bar"));
    ASSERT(string_equal(path_relative_path("foo/bar"), "foo/bar"));
    ASSERT(string_equal(path_relative_path("/"), ""));
    ASSERT(string_equal(path_relative_path("C:"), ""));
    ASSERT(string_equal(path_relative_path(""), ""));
}

TEST(test_path_has_relative_path, path)
{
    ASSERT(path_has_relative_path("/foo"));
    ASSERT(path_has_relative_path("C:/foo"));
    ASSERT(path_has_relative_path("foo"));
    ASSERT(!path_has_relative_path("/"));
    ASSERT(!path_has_relative_path("C:"));
    ASSERT(!path_has_relative_path(""));
}

TEST(test_path_filename, path)
{
    char* r;

    r = path_filename("/foo/bar.txt", allocator_temp());
    ASSERT(string_equal(r, "bar.txt"));

    r = path_filename("foo/bar.txt", allocator_temp());
    ASSERT(string_equal(r, "bar.txt"));

    r = path_filename("/foo/bar/", allocator_temp());
    ASSERT(string_equal(r, ""));

    r = path_filename("foo", allocator_temp());
    ASSERT(string_equal(r, "foo"));

    r = path_filename(".", allocator_temp());
    ASSERT(string_equal(r, "."));

    r = path_filename("..", allocator_temp());
    ASSERT(string_equal(r, ".."));

    r = path_filename("/", allocator_temp());
    ASSERT(string_equal(r, ""));
}

TEST(test_path_stem, path)
{
    char* r;

    r = path_stem("/foo/bar.txt", allocator_temp());
    ASSERT(string_equal(r, "bar"));

    r = path_stem("bar.txt", allocator_temp());
    ASSERT(string_equal(r, "bar"));

    r = path_stem("bar", allocator_temp());
    ASSERT(string_equal(r, "bar"));

    r = path_stem("/foo/.gitignore", allocator_temp());
    ASSERT(string_equal(r, ".gitignore"));

    r = path_stem(".", allocator_temp());
    ASSERT(string_equal(r, "."));

    r = path_stem("..", allocator_temp());
    ASSERT(string_equal(r, ".."));

    r = path_stem("archive.tar.gz", allocator_temp());
    ASSERT(string_equal(r, "archive.tar"));
}

TEST(test_path_extension, path)
{
    ASSERT(string_equal(path_extension("bar.txt"), ".txt"));
    ASSERT(string_equal(path_extension("bar."), "."));
    ASSERT(string_equal(path_extension("bar"), ""));
    ASSERT(string_equal(path_extension("/foo/bar.txt"), ".txt"));
    ASSERT(string_equal(path_extension("/foo/bar"), ""));
    ASSERT(string_equal(path_extension(".gitignore"), ""));
}

TEST(test_path_replace_extension, path)
{
    char* r;

    r = path_replace_extension("bar.txt", "md", allocator_temp());
    ASSERT(string_equal(r, "bar.md"));

    r = path_replace_extension("bar.txt", ".md", allocator_temp());
    ASSERT(string_equal(r, "bar.md"));

    r = path_replace_extension("bar.txt", "", allocator_temp());
    ASSERT(string_equal(r, "bar"));

    r = path_replace_extension("/foo/bar.txt", "md", allocator_temp());
    ASSERT(string_equal(r, "/foo/bar.md"));

    r = path_replace_extension("bar", "txt", allocator_temp());
    ASSERT(string_equal(r, "bar.txt"));
}

TEST(test_path_parent_path, path)
{
    char* r;

    r = path_parent_path("/foo/bar", allocator_temp());
    ASSERT(string_equal(r, "/foo"));

    r = path_parent_path("/foo/bar/", allocator_temp());
    ASSERT(string_equal(r, "/foo/bar"));

    r = path_parent_path("/foo", allocator_temp());
    ASSERT(string_equal(r, "/"));

    r = path_parent_path("foo/bar", allocator_temp());
    ASSERT(string_equal(r, "foo"));

    r = path_parent_path("foo", allocator_temp());
    ASSERT(string_equal(r, ""));

    r = path_parent_path("/", allocator_temp());
    ASSERT(string_equal(r, "/"));
}

TEST(test_path_combine, path)
{
    char* r;

    r = path_combine(allocator_temp(), "/foo", "bar", NULL);
    ASSERT(string_equal(r, "/foo/bar"));

    r = path_combine(allocator_temp(), "/foo/", "/bar", NULL);
    ASSERT(string_equal(r, "/foo/bar"));

    r = path_combine(allocator_temp(), "/foo/bar", "baz", "qux", NULL);
    ASSERT(string_equal(r, "/foo/bar/baz/qux"));

    r = path_combine(allocator_temp(), "C:", "foo", NULL);
    ASSERT(string_equal(r, "C:/foo"));
}

TEST(test_path_backslash_to_slash, path)
{
    char buf[] = "foo\\bar\\baz";
    path_backslash_to_slash(buf);
    ASSERT(string_equal(buf, "foo/bar/baz"));

    char buf2[] = "foo/bar";
    path_backslash_to_slash(buf2);
    ASSERT(string_equal(buf2, "foo/bar"));
}

TEST(test_path_slash_to_backslash, path)
{
    char buf[] = "foo/bar/baz";
    path_slash_to_backslash(buf);
    ASSERT(string_equal(buf, "foo\\bar\\baz"));

    char buf2[] = "foo\\bar";
    path_slash_to_backslash(buf2);
    ASSERT(string_equal(buf2, "foo\\bar"));
}

TEST(test_path_lexically_normal, path)
{
    char* r;

    r = path_lexically_normal("foo//bar", allocator_temp());
    ASSERT(string_equal(r, "foo/bar"));

    r = path_lexically_normal("", allocator_temp());
    ASSERT(string_equal(r, ""));

    r = path_lexically_normal(".", allocator_temp());
    ASSERT(string_equal(r, "."));
}

TEST(test_path_lexically_relative, path)
{
    char* r;

    r = path_lexically_relative("/foo/bar", "/foo", allocator_temp());
    ASSERT(string_equal(r, "bar"));

    r = path_lexically_relative("/foo", "/foo/bar", allocator_temp());
    ASSERT(string_equal(r, ".."));

    r = path_lexically_relative("/foo/bar", "/baz", allocator_temp());
    ASSERT(string_equal(r, "../foo/bar"));

    r = path_lexically_relative("/foo", "/foo", allocator_temp());
    ASSERT(string_equal(r, "."));

    r = path_lexically_relative("/foo", "foo", allocator_temp());
    ASSERT(string_equal(r, ""));
}

TEST(test_path_is_under_directory, path)
{
    ASSERT(path_is_under_directory("/foo/bar", "/foo"));
    ASSERT(!path_is_under_directory("/foo", "/foo/bar"));
    ASSERT(!path_is_under_directory("/baz", "/foo"));
    ASSERT(path_is_under_directory("/foo", "/foo"));
    ASSERT(path_is_under_directory("/foo/bar/baz", "/foo"));
}

TEST(test_path_windows_style_to_linux_relative, path)
{
    char* r;

    r = path_windows_style_to_linux_relative("/foo/bar", allocator_temp());
    ASSERT(string_equal(r, "/foo/bar"));

    r = path_windows_style_to_linux_relative("/foo", allocator_temp());
    ASSERT(string_equal(r, "/foo"));

    r = path_windows_style_to_linux_relative("C:/foo/bar", allocator_temp());
    ASSERT(string_equal(r, "C/foo/bar"));
}

TEST(test_path_parser, path)
{
    Allocator* a = allocator_temp();
    PathParser* parser;
    char* e;

    parser = path_create_parser("/foo/bar", a);
    e = path_next_element(parser);
    ASSERT(string_equal(e, "/"));
    e = path_next_element(parser);
    ASSERT(string_equal(e, "foo"));
    e = path_next_element(parser);
    ASSERT(string_equal(e, "bar"));
    e = path_next_element(parser);
    ASSERT(e == NULL);

    parser = path_create_parser("C:/foo/bar", a);
    e = path_next_element(parser);
    ASSERT(string_equal(e, "C:"));
    e = path_next_element(parser);
    ASSERT(string_equal(e, "/"));
    e = path_next_element(parser);
    ASSERT(string_equal(e, "foo"));
    e = path_next_element(parser);
    ASSERT(string_equal(e, "bar"));
    e = path_next_element(parser);
    ASSERT(e == NULL);

    parser = path_create_parser("foo", a);
    e = path_next_element(parser);
    ASSERT(string_equal(e, "foo"));
    e = path_next_element(parser);
    ASSERT(e == NULL);
}

TEST(test_path_parser_with_status, path)
{
    Allocator* a = allocator_temp();

    PathParser* parser = path_create_parser_with_status("/foo/bar", PARSE_STATUS_AFTER_ROOT_DIRECTORY, a);
    ASSERT(path_get_parse_status(parser) == PARSE_STATUS_AFTER_ROOT_DIRECTORY);

    char* e = path_next_element(parser);
    ASSERT(string_equal(e, "foo"));
    e = path_next_element(parser);
    ASSERT(string_equal(e, "bar"));
    e = path_next_element(parser);
    ASSERT(e == NULL);
}

TEST(test_path_parse_status, path)
{
    Allocator* a = allocator_temp();

    PathParser* parser = path_create_parser("/foo", a);
    ASSERT(path_get_parse_status(parser) == PARSE_STATUS_BEGIN);

    path_next_element(parser);
    ASSERT(path_get_parse_status(parser) == PARSE_STATUS_AFTER_ROOT_DIRECTORY);
}
