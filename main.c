#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"

// read the next token on the current line, return 0 if none
// thrashes any previously-returned token. make copies!
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

typedef struct _Variable {
    Type type;
    char * name;
} Variable;

typedef struct _Statement {
    // TODO/FIXME
    uint8_t z;
} Statement;

typedef struct _Block {
    char * name;
    // block args (array)
    Variable * args;
    // statements that enter into this block (array)
    Statement ** edges_in;
    // statements that transfer control flow away from themselves
    Statement ** edges_out;
    // statements within the block (array)
    Statement ** statements;
} Block;

Block new_block(void)
{
    Block block;
    memset(&block, 0, sizeof(Block));
    block.args = zero_alloc(0);
    block.edges_in = zero_alloc(0);
    block.edges_out = zero_alloc(0);
    block.statements = zero_alloc(0);
    return block;
}

typedef struct _StackSlot {
    char * name;
    size_t size;
} StackSlot;

typedef struct _Function {
    char * name;
    Type return_type;
    // args (array)
    Variable * args;
    // stack slots (array)
    StackSlot * stack_slots;
    // blocks, in order of declaration (array)
    Block ** blocks;
    // statements within the block (array)
    Statement ** statements;
} Function;

Function new_func(void)
{
    Function func;
    memset(&func, 0, sizeof(Function));
    puts("allocating args...");
    func.args = zero_alloc(0);
    puts("allocating blocks...");
    func.blocks = zero_alloc(0);
    return func;
}

typedef struct _Program {
    Function * functions;
    //Global * globals;
    //Static * statics;
} Program;

uint64_t parse_int_bare(const char * n)
{
    size_t len = strlen(n);
    
    int base = 10;
    if (len > 2 && n[0] == '0' && n[1] == 'x')
        base = 16;
    
    char * end = 0;
    uint64_t ret = (uint64_t)strtoll(n, &end, base);
    
    assert(("bare integer literal has trailing characters", (uint64_t)(end - n) == len));
    
    return ret;
}

Type parse_type(char ** b)
{
    Type type;
    memset(&type, 0, sizeof(Type));
    char * token = find_next_token(b);
    assert(token);
    if (strcmp(token, "i8") == 0)
        type.variant = TYPE_I8;
    else if (strcmp(token, "i16") == 0)
        type.variant = TYPE_I16;
    else if (strcmp(token, "i32") == 0)
        type.variant = TYPE_I32;
    else if (strcmp(token, "i64") == 0)
        type.variant = TYPE_I64;
    else if (strcmp(token, "f32") == 0)
        type.variant = TYPE_F32;
    else if (strcmp(token, "f64") == 0)
        type.variant = TYPE_F64;
    else if (strcmp(token, "{") == 0)
    {
        uint8_t is_packed = 0;
        token = find_next_token(b);
        if (strcmp(token, "packed") == 0)
        {
            is_packed = 1;
            token = find_next_token(b);
        }
        if (str_begins_with(token, "align."))
        {
            uint64_t align = parse_int_bare(token + 6);
            token = find_next_token(b);
        }
        else
            assert(("missing alignment in aggregate type spec", 0));
        
        assert(("TODO: properly parse struct types", 0));
    }
    else
    {
        assert(("invalid type", 0));
    }
    
    return type;
}

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
    
    Function current_func = new_func();
    
    while (token)
    {
        if (state == PARSER_STATE_ROOT)
        {
            if (strcmp(token, "func") == 0)
            {
                token = find_next_token(&cursor);
                assert(token);
                
                current_func.name = strcpy_z(token);
                
                token = find_next_token(&cursor);
                if (token && strcmp(token, "returns"))
                    current_func.return_type = parse_type(&cursor);
                
                state = PARSER_STATE_FUNCARGS;
            }
            else if (strcmp(token, "global") == 0)
            {
                assert(("TODO global", 0));
            }
            else if (strcmp(token, "static") == 0)
            {
                assert(("TODO static", 0));
            }
            else
            {
                printf("culprit: %s\n", token);
                assert(("unknown directive name", 0));
            }
        }
        else if (state == PARSER_STATE_BLOCKARGS)
        {
            if (strcmp(token, "arg") == 0)
            {
                puts("found block args");
                
                token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_z(token);
                Type type = parse_type(&cursor);
            }
            else
            {
                state = PARSER_STATE_FUNCSLOTS;
                continue;
            }
        }
        else if (state == PARSER_STATE_FUNCARGS)
        {
            if (strcmp(token, "arg") == 0)
            {
                puts("found func args");
                
                token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_z(token);
                size_t name_len = strlen(name);
                assert(name_len < 4096);
                Type type = parse_type(&cursor);
                
                Variable arg = {type, name};
                array_push(current_func.args, Variable, arg);
                printf("---- arg count: %zu\n", array_len(current_func.args, Variable));
                for (uint64_t i = 0; i < array_len(current_func.args, Variable); i++)
                    printf("arg name: %s\n", current_func.args[0].name);
                
                printf("pushed `%s`\n", name);
            }
            else
            {
                printf("token --- %s\n", token);
                state = PARSER_STATE_FUNCSLOTS;
                continue;
            }
        }
        else if (state == PARSER_STATE_FUNCSLOTS)
        {
            if (strcmp(token, "stack_slot") == 0)
            {
                puts("asdpoaopg");
                assert(0);
            }
            else
            {
                printf("token ~~~ %s\n", token);
                state = PARSER_STATE_BLOCK;
                continue;
            }
        }
        else if (state == PARSER_STATE_BLOCK)
        {
            if (strcmp(token, "block") != 0)
            {
                puts("agflkh9");
                assert(0);
            }
            else // new block
            {
                puts("784525746341");
                assert(0);
                state = PARSER_STATE_BLOCKARGS;
            }
        }
        
        find_end_of_line(&cursor);
        token = find_next_token_anywhere(&cursor);
    }
    
    puts("091308y4h3qjwtrkjt");
    assert(0);
    
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
