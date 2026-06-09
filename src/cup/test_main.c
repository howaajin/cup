#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern TestEntry* test_entries;
extern int num_test_entries;

char const* cur_exe_path;

static FILE* os_popen(char const* cmd, char const* mode)
{
#ifdef _WIN32
    return _popen(cmd, mode);
#else
    return popen(cmd, mode);
#endif
}

static int os_pclose(FILE* file)
{
#ifdef _WIN32
    return _pclose(file);
#else
    return pclose(file);
#endif
}

static void print_entries(void)
{
    putchar('[');
    int b_first = 1;
    for (int i = 0; i != num_test_entries; i++)
    {
        TestEntry const* e = &test_entries[i];
        if (b_first)
        {
            b_first = 0;
        }
        else
        {
            putchar(',');
        }
        puts("\n    {");
        printf("        \"name\": \"%s\",\n", e->name);
        printf("        \"group\": \"%s\",\n", e->group);
        printf("        \"file\": \"%s\",\n", e->file);
        printf("        \"line\": %d\n", e->line);
        printf("    }");
    }
    puts("\n]");
}

static void run_test(char const* name)
{
    for (int i = 0; i != num_test_entries; i++)
    {
        TestEntry const* e = &test_entries[i];
        if (strcmp(e->name, name) == 0)
        {
            e->fn();
            break;
        }
    }
}

#define MAX_JOBS 8

static bool run_all_tests()
{
    struct
    {
        FILE* pipe;
        size_t output_size;
        size_t output_capacity;
        char const* file;
        int line;
        char* output;
    } slots[MAX_JOBS] = {0};
    size_t next_test = 0;
    size_t num_running = 0;
    bool has_error = false;
    while (true)
    {
        while (num_running != MAX_JOBS && next_test != (size_t)num_test_entries)
        {
            size_t i = -1;
            while (slots[++i].pipe);
            assert(i != MAX_JOBS);
            char cmdbuffer[4096];
            TestEntry* e = &test_entries[next_test++];
            snprintf(cmdbuffer, sizeof(cmdbuffer), "\"%s\" %s 2>&1", cur_exe_path, e->name);
            slots[i].pipe = os_popen(cmdbuffer, "r");
            slots[i].file = e->file;
            slots[i].line = e->line;
            slots[i].output_size = 0;
            slots[i].output_capacity = 4096;
            slots[i].output = malloc(slots[i].output_capacity);
            assert(slots[i].output);
            num_running += 1;
        }
        for (size_t i = 0; i != MAX_JOBS; i++)
        {
            if (slots[i].pipe)
            {
                char buffer[1024];
                size_t n = fread(buffer, 1, sizeof(buffer), slots[i].pipe);
                if (n)
                {
                    if (slots[i].output_size + n >= slots[i].output_capacity)
                    {
                        slots[i].output_capacity = slots[i].output_capacity * 2 + n;
                        slots[i].output = realloc(slots[i].output, slots[i].output_capacity);
                        assert(slots[i].output);
                    }
                    memcpy(slots[i].output + slots[i].output_size, buffer, n);
                    slots[i].output_size += n;
                }
                if (feof(slots[i].pipe))
                {
                    int exit_code = os_pclose(slots[i].pipe);
                    slots[i].pipe = NULL;
                    if (!has_error && exit_code != EXIT_SUCCESS)
                    {
                        has_error = true;
                    }
                    printf("TEST %s:%d EXIT: %d\n", slots[i].file, slots[i].line, exit_code);
                    if (slots[i].output_size)
                    {
                        printf("OUTPUT:\n");
                        printf("%.*s\n", (int)slots[i].output_size, slots[i].output);
                    }
                    free(slots[i].output);
                    num_running -= 1;
                }
            }
        }
        if (num_running == 0 && next_test == (size_t)num_test_entries)
        {
            break;
        }
    }
    return has_error;
}

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main(int argc, char** argv)
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);
    _CrtSetReportMode(_CRT_WARN, 0);
#endif

    cur_exe_path = argv[0];
    if (argc == 1)
    {
        if (run_all_tests())
        {
            return EXIT_FAILURE;
        }
        else
        {
            return EXIT_SUCCESS;
        }
    }
    if (argc == 2)
    {
        if (strcmp(argv[1], "-print") == 0)
        {
            print_entries();
            return 0;
        }
        run_test(argv[1]);
        return 0;
    }
    return 1;
}