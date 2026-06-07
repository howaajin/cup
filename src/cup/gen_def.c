#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const* excluded_keywords[] = {
    "static",
    "typedef",
    "struct",
    "union",
    "enum",
};

static bool starts_with_keyword(char const* p, char const* kw)
{
    size_t n = strlen(kw);
    return memcmp(p, kw, n) == 0 && (isspace((unsigned char)p[n]) || p[n] == '\0');
}

static void skip_bom(FILE* file)
{
    uint8_t ch[3];
    if (fread(ch, 1, 3, file) != 3) return;
    if (ch[0] != 0xEF || ch[1] != 0xBB || ch[2] != 0xBF) fseek(file, 0, SEEK_SET);
}

static int count_braces(char const* s)
{
    int n = 0;
    for (; *s; s++)
    {
        if (*s == '{') n++;
        if (*s == '}') n--;
    }
    return n;
}

static int compare_strings(void const* a, void const* b)
{
    return strcmp(*(char const**)a, *(char const**)b);
}

static char const* skip_space(char const* p)
{
    while (isspace((unsigned char)*p)) p++;
    return p;
}

static bool try_extract_export(char const* decl, char* name, size_t name_cap)
{
    char const* p = skip_space(decl);
    if (*p == '\0' || *p == '#' || *p == '}' || *p == ';')
        return false;

    for (size_t i = 0; i < sizeof(excluded_keywords) / sizeof(excluded_keywords[0]); i++)
    {
        if (starts_with_keyword(p, excluded_keywords[i])) return false;
    }

    // Must have '(' , ')' , and ';' (complete declaration)
    char const* lparen = strchr(p, '(');
    char const* rparen = strchr(p, ')');
    char const* semi = strchr(p, ';');

    // Reject function pointer variables "void (*name)(...)"
    // but allow function pointer params "void func(void (*cb)(...))"
    if (lparen && lparen[1] == '*') return false;
    if (!lparen || !rparen || !semi) return false;
    // Must NOT be a function definition (opening '{' before ';')
    char const* brace = strchr(p, '{');
    if (brace && brace < semi) return false;

    // Extract identifier before '('
    p = lparen;
    while (p > decl && isspace((unsigned char)p[-1])) --p;

    char const* end = p;
    while (p > decl && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) --p;

    size_t len = end - p;
    if (len == 0 || len >= name_cap) return false;

    memcpy(name, p, len);
    name[len] = '\0';

    // Reject compiler annotation names like __declspec, __attribute__
    // These are not real functions, just modifiers before a declaration
    if (name[0] == '_' && name[1] == '_') return false;

    return true;
}

int main(int argc, char** argv)
{
    char const* input_path = NULL;
    char const* output_path = NULL;
    char const* program = argv ? *argv : "";
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            output_path = argv[++i];
        }
        else if (argv[i][0] != '-')
        {
            input_path = argv[i];
        }
    }
    if (!input_path || !output_path)
    {
        fprintf(stderr, "usage: %s [-o output] input\n", program);
        return EXIT_FAILURE;
    }

    FILE* file = fopen(input_path, "rb");
    if (!file)
    {
        fprintf(stderr, "cannot open file: %s\n", input_path);
        return EXIT_FAILURE;
    }
    skip_bom(file);

    char** names = NULL;
    size_t names_count = 0, names_cap = 0;
    char decl[65536] = "";
    int brace_depth = 0;
    char line[16384];

    while (fgets(line, sizeof(line), file))
    {
        size_t line_len = strlen(line);
        while (line_len > 0 && (line[line_len - 1] == '\r' || line[line_len - 1] == '\n'))
        {
            line[--line_len] = '\0';
        }

        char* p = line;
        while (*p && isspace((unsigned char)*p)) ++p;

        if (brace_depth > 0)
        {
            brace_depth += count_braces(p);
            continue;
        }

        // Reset decl on empty lines, directives, comments, or continuation lines
        if (*p == '\0' || *p == '#' || (p[0] == '/' && (p[1] == '/' || p[1] == '*')))
        {
            decl[0] = '\0';
            continue;
        }

        size_t tlen = strlen(p);
        if (tlen > 0 && p[tlen - 1] == '\\') continue;

        // Append to declaration buffer
        if (decl[0]) strcat(decl, " ");
        strcat(decl, p);

        // Check if we have a complete declaration
        char* semi = strchr(decl, ';');
        char* brace = strchr(decl, '{');
        if (semi && (!brace || semi < brace))
        {
            char name[4096];
            if (try_extract_export(decl, name, sizeof(name)))
            {
                // Dedup linear scan (n is small)
                bool dup = false;
                for (size_t i = 0; i < names_count; i++)
                {
                    if (strcmp(names[i], name) == 0)
                    {
                        dup = true;
                        break;
                    }
                }
                if (!dup)
                {
                    if (names_count == names_cap)
                    {
                        names_cap = names_cap ? names_cap * 2 : 1024;
                        names = realloc(names, names_cap * sizeof(char*));
                    }
                    names[names_count] = malloc(strlen(name) + 1);
                    strcpy(names[names_count], name);
                    names_count++;
                }
            }
            decl[0] = '\0';
        }
        else if (brace)
        {
            brace_depth = 1 + count_braces(brace + 1);
            decl[0] = '\0';
        }
    }

    fclose(file);

    if (names_count > 0) qsort(names, names_count, sizeof(char*), compare_strings);

    FILE* outfile = fopen(output_path, "wb");
    if (!outfile)
    {
        fprintf(stderr, "cannot open output file: %s\n", output_path);
        return EXIT_FAILURE;
    }
    fprintf(outfile, "NAME cup.exe\n");
    fprintf(outfile, "EXPORTS\n");
    for (size_t i = 0; i < names_count; i++)
    {
        fprintf(outfile, "    %s\n", names[i]);
        free(names[i]);
    }
    fclose(outfile);
    free(names);

    return EXIT_SUCCESS;
}
