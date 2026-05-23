#include "cup/depfile.h"
#include "core/allocator.h"
#include "core/array.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void depfile_eat_chars(FILE* file, size_t n)
{
    fseek(file, (long)n, SEEK_CUR);
}

static void depfile_peek_chars(FILE* file, size_t n, char* out)
{
    long pos = ftell(file);
    size_t read = fread(out, 1, n, file);
    if (read != n)
    {
        memset(out + read, EOF, n - read);
    }
    fseek(file, pos, SEEK_SET);
}

static void depfile_skip_spaces_and_continuations(FILE* f)
{
    for (;;)
    {
        char ch[2];
        depfile_peek_chars(f, 2, ch);

        if (ch[0] == ' ' || ch[0] == '\t')
        {
            depfile_eat_chars(f, 1);
            continue;
        }

        if (ch[0] == '\\' && (ch[1] == '\r' || ch[1] == '\n'))
        {
            depfile_eat_chars(f, 1);
            depfile_peek_chars(f, 1, ch);
            if (ch[0] == '\r') depfile_eat_chars(f, 1);
            depfile_peek_chars(f, 1, ch);
            if (ch[0] == '\n') depfile_eat_chars(f, 1);
            continue;
        }

        break;
    }
}

static bool is_variable_assignment_line(FILE* f)
{
    long original_pos = ftell(f);
    bool is_assignment = false;
    char ch;

    while (fread(&ch, 1, 1, f) == 1)
    {
        if (ch == '\n' || ch == '\r') break;
        if (ch == ':') break;
        if (ch == '=')
        {
            is_assignment = true;
            break;
        }
    }

    fseek(f, original_pos, SEEK_SET);
    return is_assignment;
}

static void depfile_skip_current_line(FILE* f)
{
    char ch;
    while (fread(&ch, 1, 1, f) == 1)
    {
        if (ch == '\n') break;
    }
}

void depfile_parser_init(DepfileParser* parser, FILE* f)
{
    parser->file = f;
    parser->state = 0;
    parser->is_phony_rule = false;
}

bool depfile_parser_next(DepfileParser* p, Allocator* allocator, char** out_path, DepfileItemType* out_type)
{
    for (;;)
    {
        if (p->state == 0)
        {
            depfile_skip_spaces_and_continuations(p->file);
            char peek_eof;
            long pos = ftell(p->file);
            if (fread(&peek_eof, 1, 1, p->file) == 0) return false;
            fseek(p->file, pos, SEEK_SET);
            if (is_variable_assignment_line(p->file))
            {
                depfile_skip_current_line(p->file);
                continue;
            }
        }
        depfile_skip_spaces_and_continuations(p->file);

        char ch[2];
        depfile_peek_chars(p->file, 2, ch);

        if (ch[0] == EOF)
        {
            return false;
        }

        if (ch[0] == ':')
        {
            depfile_eat_chars(p->file, 1);
            p->state = 1;
            continue;
        }

        if (ch[0] == '|')
        {
            depfile_eat_chars(p->file, 1);
            p->state = 2;
            continue;
        }

        if (ch[0] == '\n' || ch[0] == '\r')
        {
            if (ch[0] == '\r') depfile_eat_chars(p->file, 1);
            depfile_peek_chars(p->file, 1, ch);
            if (ch[0] == '\n') depfile_eat_chars(p->file, 1);

            p->state = 0;
            p->is_phony_rule = false;
            continue;
        }

        bool word_read = false;
        for (;;)
        {
            char next[2];
            depfile_peek_chars(p->file, 2, next);

            if (next[0] == (char)EOF || isspace(next[0]) || next[0] == '|')
            {
                break;
            }

            if (next[0] == ':')
            {
                if (next[1] == '/' || next[1] == '\\')
                {
                    array_push(allocator, *out_path, ':');
                    depfile_eat_chars(p->file, 1);
                    word_read = true;
                    continue;
                }
                else
                {
                    break;
                }
            }
            if (next[0] == '\\')
            {
                if (next[1] == ' ')
                {
                    depfile_eat_chars(p->file, 2);
                    array_push(allocator, *out_path, ' ');
                    word_read = true;
                    continue;
                }
                if (next[1] == '\r' || next[1] == '\n')
                {
                    break;
                }

                depfile_eat_chars(p->file, 1);
                array_push(allocator, *out_path, '/');
                word_read = true;
                continue;
            }

            array_push(allocator, *out_path, next[0]);
            depfile_eat_chars(p->file, 1);
            word_read = true;
        }

        if (word_read)
        {
            array_push(allocator, *out_path, 0);
            array_pop(*out_path);

            if (p->state == 0)
            {
                *out_type = DEPFILE_ITEM_TARGET;

                if (strcmp(*out_path, ".PHONY") == 0)
                {
                    p->is_phony_rule = true;
                }
            }
            else if (p->state == 1)
            {
                *out_type = p->is_phony_rule ? DEPFILE_ITEM_PHONY : DEPFILE_ITEM_NORMAL_DEP;
            }
            else
            {
                *out_type = DEPFILE_ITEM_ORDER_ONLY_DEP;
            }

            return true;
        }
    }
}
