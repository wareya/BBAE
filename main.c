#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"
#include "emitter.h"

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

Type basic_type(int x)
{
    Type ret;
    memset(&ret, 0, sizeof(Type));
    ret.variant = x;
    return ret;
}

typedef struct _Global {
    char * name;
    Type type;
    struct _Global * next;
} Global;

enum {
    VALUE_NONE,
    VALUE_CONST,
    VALUE_SSA,
    VALUE_ARG,
};

struct _Statement;
typedef struct _Value {
    int variant; // values can be constants, SSA values produced by operations, or func/block arguments
    Type type;
    uint64_t constant; // is a pointer if type is aggregate. otherwise is bits of value
    struct _Statement * ssa;
    char * arg;
    
    uint8_t regalloced;
    uint64_t regalloc; // if highest bit set: on stack. else: register id from abi.h.
} Value;

typedef struct _Variable {
    char * name;
    Type type;
    struct _Statement ** edges_out;
} Variable;

typedef union _Operand {
    Type type;
    Value value;
    char * text;
} Operand;

struct _Block;
typedef struct _Statement {
    char * output_name; // if null, instruction. else, operation.
    char * statement_name;
    struct _Block * block;
    // array
    Operand * args;
    // secondary args -- ONLY for if and if-else.
    Operand * args_a;
    Operand * args_b;
    // array
    struct _Statement ** edges_in;
    // array
    struct _Statement ** edges_out;
} Statement;

Statement * new_statement(void)
{
    Statement * statement = (Statement *)zero_alloc(sizeof(Statement));
    statement->args = (Operand *)zero_alloc(0);
    statement->edges_in = (Statement **)zero_alloc(0);
    statement->edges_out = (Statement **)zero_alloc(0);
    return statement;
}
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

Block * new_block(void)
{
    Block * block = zero_alloc(sizeof(Block));
    block->args = (Variable *)zero_alloc(0);
    block->edges_in = (Statement **)zero_alloc(0);
    block->edges_out = (Statement **)zero_alloc(0);
    block->statements = (Statement **)zero_alloc(0);
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
    // explicit pointer to entry block so that it doesn't get lost
    Block * entry_block;
} Function;

Function new_func(void)
{
    Function func;
    memset(&func, 0, sizeof(Function));
    func.args = (Variable *)zero_alloc(0);
    func.stack_slots = (StackSlot *)zero_alloc(0);
    func.blocks = (Block **)zero_alloc(0);
    return func;
}

typedef struct _Program {
    // array
    Function * functions;
    //Global * globals;
    //Static * statics;
} Program;

int char_value(const char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return -1;
}

// truncating instead of clamping (discards upper bits); does not set errno
uint64_t my_strtoull(const char * str, const char ** endptr)
{
    uint8_t negative = 0;
    if (str[0] == '-')
    {
        negative = 1;
    }
    
    int radix = 10;
    if (str[0] == '0' && str[1] == 'x')
    {
        str += 2;
        radix = 16;
    }
    
    uint64_t out = 0;
    while (*str != 0 && char_value(*str) >= 0 && char_value(*str) < radix)
    {
        out *= radix;
        out += char_value(*str);
        str++;
    }
    
    if (endptr)
        *endptr = str;
    
    return negative ? -out : out;
}

uint64_t parse_int_bare(const char * n)
{
    size_t len = strlen(n);
    
    const char * end = 0;
    uint64_t ret = (uint64_t)my_strtoull(n, &end);
    assert(("bare integer literal has trailing characters", (uint64_t)(end - n) == len));
    
    return ret;
}

uint64_t parse_int_nonbare(const char * n)
{
    const char * end = 0;
    uint64_t ret = (uint64_t)my_strtoull(n, &end);
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

Program program;
Function current_func;
Block * current_block = 0;

Value parse_value(char * token)
{
    Value ret;
    memset(&ret, 0, sizeof(Value));
    
    /*
typedef struct _Value {
    int variant; // values can be constants, SSA values produced by operations, or func/block arguments
    Type type;
    uint64_t constant; // is a pointer if type is aggregate. otherwise is bits of value
    struct _Statement * ssa;
    char * arg;
} Value;
    */

    assert(token);
    // tokens
    assert(strcmp(token, "=") != 0);
    assert(strcmp(token, "{") != 0);
    assert(strcmp(token, "}") != 0);
    assert(strcmp(token, "<=") != 0);
    
    if ((token[0] >= '0' && token[0] <= '9')
        || token[0] == '-'|| token[0] == '.')
    {
        if (str_ends_with(token, "f32"))
        {
            char * end = 0;
            float f = strtof(token, &end);
            assert(("invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            ret.variant = VALUE_CONST;
            ret.type = basic_type(TYPE_F32);
            memcpy(&ret.constant, &f, 4);
        }
        else if (str_ends_with(token, "f64"))
        {
            char * end = 0;
            double f = strtod(token, &end);
            assert(("invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            ret.variant = VALUE_CONST;
            ret.type = basic_type(TYPE_F64);
            memcpy(&ret.constant, &f, 8);
        }
        else if (str_ends_with(token, "i8") || str_ends_with(token, "i16")
                 || str_ends_with(token, "i32") || str_ends_with(token, "i64"))
        {
            uint8_t suffix_len = str_ends_with(token, "i8") ? 2 : 3;
            uint64_t n = parse_int_nonbare(token);
        }
        else
        {
            assert(("unknown type of literal value", 0));
        }
    }
    
    return ret;
}

uint8_t op_is_v_v(char * opname)
{
    const char * ops[] = {
        "add", "sub", "mul", "imul", "div", "idiv", "rem", "irem", "div_unsafe", "idiv_unsafe", "rem_unsafe", "irem_unsafe",
        "shl", "shr", "shr_unsafe", "sar", "sar_unsafe", "and", "or", "xor",
        "cmp_eq", "cmp_ne", "cmp_ge", "cmp_le", "cmp_g", "cmp_l",
        "icmp_ge", "icmp_le", "icmp_g", "icmp_l",
        "fcmp_eq", "fcmp_ne", "fcmp_ge", "fcmp_le", "fcmp_g", "fcmp_l",
        "addf", "subf", "mulf", "divf", "remf",
        "ptralias", "ptralias_merge", "ptralias_disjoint",
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(char *); i++)
    {
        if (strcmp(opname, ops[i]) == 0)
            return 1;
    }
    return 0;
}
uint8_t op_is_v_v_v(char * opname)
{
    const char * ops[] = {
        "ternary", "inject"
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(char *); i++)
    {
        if (strcmp(opname, ops[i]) == 0)
            return 1;
    }
    return 0;
}

Statement * parse_statement(char ** cursor)
{
    Statement * ret = new_statement();
    char * token = strcpy_z(find_next_token(cursor));
    char * cursor_before_token2 = *cursor;
    char * token2 = find_next_token(cursor);
    
    if (token2 && strcmp("=", token2) == 0)
    {
        ret->output_name = token;
        ret->statement_name = strcpy_z(find_next_token(cursor));
        
        if (op_is_v_v(ret->statement_name))
        {
            char * op1 = strcpy_z(find_next_token(cursor));
            char * op2 = strcpy_z(find_next_token(cursor));
        }
        // TODO: operators
        assert(("glfy39", 0));
    }
    else
    {
        *cursor = cursor_before_token2;
        
        ret->statement_name = token;
        
        if (strcmp(ret->statement_name, "return") == 0)
        {
            if (token2)
            {
                Operand op;
                op.value = parse_value(find_next_token(cursor));
                array_push(ret->args, Operand, op);
            }
        }
        // TODO: other statements
        else
        {
            printf("culprit: %s\n", ret->statement_name);
            assert(("unknown instruction", 0));
        }
    }
    
    return ret;
}

int parse_file(char * cursor)
{
    int state = PARSER_STATE_ROOT;
    char * token = find_next_token_anywhere(&cursor);
    
    program.functions = (Function *)zero_alloc(0);
    current_func = new_func();
    
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
                token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_z(token);
                Type type = parse_type(&cursor);
                
                Variable arg = {name, type, 0};
                array_push(current_func.args, Variable, arg);
            }
            else
            {
                state = PARSER_STATE_FUNCSLOTS;
                continue;
            }
        }
        else if (state == PARSER_STATE_FUNCSLOTS)
        {
            if (strcmp(token, "stack_slot") == 0)
            {
                token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_z(token);
                
                token = find_next_token(&cursor);
                uint64_t size = parse_int_bare(token);
                
                StackSlot slot = {name, size};
                array_push(current_func.stack_slots, StackSlot, slot);
            }
            else
            {
                current_block = new_block();
                array_push(current_func.blocks, Block *, current_block);
                current_func.entry_block = current_block;
                
                state = PARSER_STATE_BLOCK;
                continue;
            }
        }
        else if (state == PARSER_STATE_BLOCK)
        {
            if (strcmp(token, "block") != 0 && strcmp(token, "endfunc") != 0)
            {
                cursor -= strlen(token);
                Statement * statement = parse_statement(&cursor);
                array_push(current_block->statements, Statement *, statement);
            }
            else if (strcmp(token, "block") == 0)
            {
                // new block
                puts("784525746341");
                assert(0);
                state = PARSER_STATE_BLOCKARGS;
            }
            else if (strcmp(token, "endfunc") == 0)
            {
                array_push(program.functions, Function, current_func);
                current_func = new_func();
                // end of function
            }
            else
            {
                printf("culprit: %s\n", token);
                assert(("unknown statement or directive", 0));
            }
        }
        
        find_end_of_line(&cursor);
        token = find_next_token_anywhere(&cursor);
    }
    
    puts("finished parsing program!");
    
    return 0;
}

byte_buffer * code;
void compile_file(void)
{
    
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
    
    parse_file(buffer);
    
    return 0;
}
