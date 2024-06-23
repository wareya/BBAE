#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

union CMaxAlignT
{
    int a;
    long b;
    long * c;
    long long d;
    void * e;
    void (*f)(void);
    long double (*g)(long double, long double);
    long double h;
    uint8_t * i;
};

void * alloc_list = 0;
void * zero_alloc(size_t n)
{
    size_t prefix_size = sizeof(union CMaxAlignT);
    if (prefix_size < 2 * sizeof(uint8_t **))
        prefix_size *= 2;
    n += prefix_size;
    
    uint8_t * alloc = (uint8_t *)calloc(1, n);
    assert(alloc);
    
    uint8_t ** alloc_next = (uint8_t **)alloc;
    *alloc_next = alloc_list;
    uint8_t ** alloc_prev = ((uint8_t **)alloc_list) + 1;
    *alloc_prev = alloc;
    
    alloc_list = alloc;
    uint8_t * ret = alloc + prefix_size;
    return ret;
}
void * zero_realloc(void * buf, size_t n)
{
    size_t prefix_size = sizeof(union CMaxAlignT);
    if (prefix_size < 2 * sizeof(uint8_t **))
        prefix_size *= 2;
    
    void * raw_buf = (void *)(((uint8_t *)buf) - prefix_size);
    
    // FIXME/TODO
}
void free_all_compiler_allocs(void)
{
    while (alloc_list)
    {
        uint8_t * alloc_next = *(uint8_t **)alloc_list;
        free(alloc_list);
        alloc_list = alloc_next;
    }
}
// Returns a pointer to a buffer with N+1 bytes reserved, and at least one null terminator.
char * strcpy_len(char * str, size_t len)
{
    if (!str)
        return (char *)zero_alloc(1);
    char * ret = (char *)zero_alloc(len + 1);
    assert(ret);
    char * ret_copy = ret;
    char c = 0;
    while ((c = *str++) && len-- > 0)
        *ret_copy++ = c;
    return ret;
}
char * strcpy_n(char * str)
{
    size_t len = strlen(str);
    return strcpy_len(str, len);
}

uint8_t is_space(char c)
{
    return c == '\r' || c == '\n';
}
void skip_space(char ** b)
{
    while (is_space(**b))
        *b += 1;
}
void to_space(char ** b)
{
    while (!is_space(**b))
        *b += 1;
}
uint8_t is_newline(char c)
{
    return c == '\r' || c == '\n';
}
void skip_newline(char ** b)
{
    while (is_newline(**b))
        *b += 1;
}
void to_newline(char ** b)
{
    while (!is_newline(**b))
        *b += 1;
}

uint8_t is_comment(char * b)
{
    if (b == 0 || b[0] == 0)
        return 0;
    if (b[0] == '/' && b[1] == '/')
        return 1;
    if (b[0] == '#')
        return 1;
    return 0;
}
// read the next token on the current line, return 0 if none
// thrashes any previously-returned token
char token[4096];
size_t token_len;
char * find_next_token(char ** b)
{
    memset(token, 0, 4096);
    
    char * w = token;
    
    // skip leading spaces
    while (is_space(**b))
    {
        *b += 1;
        if (**b == 0)
            return 0;
    }
    
    while (!is_newline(**b) && !is_space(**b) && **b != 0 && token_len < 4096 && !is_comment(*b))
    {
        *w = **b;
        w += 1;
        *b += 1;
        token_len += 1;
    }
    
    assert(token[4095] == 0);
    
    if (token[0] != 0)
        return token;
    else
        return 0;
}
// find the next token even if it's on a different line
// thrashes any previously-returned token
char * find_next_token_anywhere(char ** b)
{
    while ((is_newline(**b) || is_space(**b)) && **b != 0)
    {
        *b += 1;
        if (is_comment(*b))
        {
            while (!is_newline(**b) && **b != 0)
                *b += 1;
        }
    }
    
    return find_next_token(b);
}
// TODO: emit a warning or error if other tokens are encountered
void find_end_of_line(char ** b)
{
    while (!is_newline(**b) && **b != 0)
        *b += 1;
}

enum {
    TYPE_NONE,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_F32,
    TYPE_F64,
    TYPE_AGG,
};

typedef struct _AggData {
    size_t align;
    size_t size;
    uint8_t * per_byte_likeness; // pointer to an array of uint8_ts with size `size`
    uint8_t packed;
} AggData;

typedef struct _Type {
    int variant;
    AggData aggdata;
} Type;

typedef struct _Global {
    char * name;
    Type type;
    struct _Global * next;
} Global;

typedef struct _Function {
    Type return_type;
    size_t arg_count;
    Type * args;
    uint8_t n;
} Function;

typedef struct _Program {
    Function * functions;
    //Global * globals;
    //Static * statics;
} Program;

enum {
    PARSER_STATE_ROOT,
    PARSER_STATE_FUNCARGS,
    PARSER_STATE_FUNCSLOTS,
    PARSER_STATE_BLOCKARGS,
    PARSER_STATE_BLOCK,
};

int parse(char * cursor)
{
    int state = PARSER_STATE_ROOT;
    char * token = find_next_token_anywhere(&cursor);
    
    while (token)
    {
        if (state == PARSER_STATE_ROOT)
        {
            if (strcmp(token, "func") == 0)
            {
                char * token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_n(token);
                token = find_next_token(&cursor);
                //if (token && strcmp(token, "returns"))
            }
            else if (strcmp(token, "global") == 0)
            {
                assert(("TODO global", 0));
            }
            else if (strcmp(token, "static") == 0)
            {
                assert(("TODO static", 0));
            }
        }
    }
    
    return 0;
}

int main(int argc, char ** argv)
{
    if (argc < 2)
        return puts("please provide file"), 0;
    
    FILE * f = fopen(argv[1], "rb");
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek (f, 0, SEEK_SET);
    
    char * buffer = (char *)malloc(length+1);
    if (!buffer)
        puts("failed to allocate memory"), exit(-1);
    
    size_t n = fread(buffer, 1, length, f);
    assert(n == (size_t)length);
    buffer[length] = 0;
    fclose(f);
    
    parse(buffer);
    
    return 0;
}