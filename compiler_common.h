#ifndef BBAE_COMPILER_COMMON
#define BBAE_COMPILER_COMMON

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"

// read the next token on the current line, return 0 if none
// thrashes any previously-returned token. make copies!
static char token[4096];
static size_t token_len;
static char * find_next_token(char ** b)
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
static char * find_next_token_anywhere(char ** b)
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
static void find_end_of_line(char ** b)
{
    while (!is_newline(**b) && **b != 0)
        *b += 1;
}

static uint64_t size_guess_align(uint64_t s)
{
    uint64_t ret = 1;
    while (ret < 64 && ret < s)
        ret <<= 1;
    return ret;
}

static int char_value(const char c)
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
static uint64_t my_strtoull(const char * str, const char ** endptr)
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

enum BBAE_TYPE_VARIANT {
    TYPE_INVALID,
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
    enum BBAE_TYPE_VARIANT variant;
    AggData aggdata;
} Type;

static Type basic_type(int x)
{
    Type ret;
    memset(&ret, 0, sizeof(Type));
    ret.variant = x;
    return ret;
}

static uint8_t type_is_basic(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_F64;
}
static uint8_t type_is_int(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_I64;
}
static uint8_t type_is_float(Type type)
{
    return type.variant >= TYPE_F32 && type.variant <= TYPE_F64;
}

static size_t type_size(Type type)
{
    if (type.variant == TYPE_AGG)
        return type.aggdata.size;
    else if (type.variant == TYPE_I8)
        return 1;
    else if (type.variant == TYPE_I16)
        return 2;
    else if (type.variant == TYPE_I32)
        return 4;
    else if (type.variant == TYPE_I64)
        return 8;
    else if (type.variant == TYPE_F32)
        return 4;
    else if (type.variant == TYPE_F64)
        return 8;
    else
        assert(0);
}

typedef struct _Global {
    char * name;
    Type type;
    struct _Global * next;
} Global;

enum BBAE_VALUE_VARIANT {
    VALUE_INVALID,
    VALUE_CONST,
    VALUE_SSA,
    VALUE_ARG,
    VALUE_STACKADDR,
};

struct _Statement;
struct _StackSlot;
typedef struct _Value {
    enum BBAE_VALUE_VARIANT variant; // values can be constants, SSA values produced by operations, or func/block arguments
    Type type;
    uint64_t constant; // is a pointer if type is aggregate. otherwise is bits of value
    struct _Statement * ssa;
    char * arg;
    struct _StackSlot * slotinfo;
    
    struct _Statement ** edges_out;
    //struct _Statement * last_edge;
    
    // number of outward edges that have been used
    // used during register allocation to estimate live range
    size_t alloced_use_count;
    
    uint8_t regalloced; // 1 if regalloced already, 0 otherwise
    // if highest bit set: on stack, lower bits are stack slot id
    // else: register id from abi.h
    // if -1: no allocation, literal
    uint64_t regalloc;
    // regalloced and regalloc are set once and never changed
    
    struct _StackSlot * spilled; // set if currently spilled, null otherwise
    // set once when spilled. when unspilling, Value needs to be duplicated, modified, and injected into descendants.
} Value;

enum BBAE_OP_VARIANT {
    OP_KIND_INVALID,
    OP_KIND_TYPE,
    OP_KIND_VALUE,
    OP_KIND_TEXT,
};

typedef struct _Operand {
    enum BBAE_OP_VARIANT variant;
    Type rawtype;
    Value * value;
    char * text;
} Operand;

struct _Block;
typedef struct _Statement {
    char * output_name; // if null, instruction. else, operation.
    Value * output; // if null, instruction. else, operation.
    char * statement_name;
    struct _Block * block;
    
    uint64_t num; // only used during register allocation; zero until then
    
    // array
    Operand * args;
    // arrays of secondary args -- ONLY for if and if-else.
    Operand * args_a;
    Operand * args_b;
} Statement;

static Statement * new_statement(void)
{
    Statement * statement = (Statement *)zero_alloc(sizeof(Statement));
    statement->args = (Operand *)zero_alloc(0);
    statement->args_a = (Operand *)zero_alloc(0);
    statement->args_b = (Operand *)zero_alloc(0);
    return statement;
}

// stack slots are allocated on a per-block basis
typedef struct _SlotAllocInfo {
    Value * value;
    uint8_t used;
} SlotAllocInfo;

typedef struct _Block {
    char * name;
    // block args (array)
    Value ** args;
    // statements that enter into this block (array)
    Statement ** edges_in;
    // statements that transfer control flow away from themselves
    Statement ** edges_out;
    // statements within the block (array)
    Statement ** statements;
} Block;

static Block * new_block(void)
{
    Block * block = zero_alloc(sizeof(Block));
    block->args = (Value **)zero_alloc(0);
    block->edges_in = (Statement **)zero_alloc(0);
    block->edges_out = (Statement **)zero_alloc(0);
    block->statements = (Statement **)zero_alloc(0);
    return block;
}

typedef struct _StackSlot {
    char * name;
    size_t size;
    int64_t offset;
    uint64_t align;
} StackSlot;

typedef struct _Function {
    char * name;
    Type return_type;
    // args (array)
    Value ** args;
    // stack slots (array)
    Value ** stack_slots;
    // blocks, in order of declaration (array)
    Block ** blocks;
    // explicit pointer to entry block so that it doesn't get lost
    Block * entry_block;
} Function;

static Function new_func(void)
{
    Function func;
    memset(&func, 0, sizeof(Function));
    func.args = (Value **)zero_alloc(0);
    func.stack_slots = (Value **)zero_alloc(0);
    func.blocks = (Block **)zero_alloc(0);
    return func;
}

typedef struct _Program {
    // array
    Function * functions;
    //Global * globals;
    //Static * statics;
} Program;

static uint64_t parse_int_bare(const char * n)
{
    size_t len = strlen(n);
    
    const char * end = 0;
    uint64_t ret = (uint64_t)my_strtoull(n, &end);
    assert(("bare integer literal has trailing characters", (uint64_t)(end - n) == len));
    
    return ret;
}

static uint64_t parse_int_nonbare(const char * n)
{
    const char * end = 0;
    uint64_t ret = (uint64_t)my_strtoull(n, &end);
    return ret;
}

static Type parse_type(char ** b)
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

static Value * make_value(Type type)
{
    Value * ret = (Value *)zero_alloc(sizeof(Value));
    ret->edges_out = (Statement **)zero_alloc(0);
    ret->type = type;
    return ret;
}

static Value * make_const_value(enum BBAE_TYPE_VARIANT variant, uint64_t data)
{
    Value * ret = make_value(basic_type(variant));
    ret->variant = VALUE_CONST;
    ret->constant = data;
    return ret;
}

static Value * make_stackslot_value(StackSlot * slot)
{
    Value * ret = make_value(basic_type(TYPE_I64));
    ret->variant = VALUE_STACKADDR;
    ret->slotinfo = slot;
    return ret;
}

static Value * add_stack_slot(Function * func, char * name, uint64_t size)
{            
    StackSlot _slot = {name, size, 0, 0};
    StackSlot * slot = (StackSlot *)zero_alloc(sizeof(StackSlot));
    *slot = _slot;
    Value * val = make_stackslot_value(slot);
    array_push(func->stack_slots, Value *, val);
    return val;
}

static Operand new_op_val(Value * val)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_VALUE;
    op.value = val;
    return op;
}

static uint64_t temp_ctr = 0;
static char * make_temp_var_name(void)
{
    size_t len = snprintf(0, 0, "__bbae_temp_%llu", temp_ctr);
    char * str = zero_alloc(len + 1);
    snprintf(str, len, "__bbae_temp_%llu", temp_ctr);
    return str;
}

static void add_statement_output(Statement * statement)
{
    if (statement->output_name)
    {
        if (strcmp(statement->statement_name, "add") == 0 ||
            strcmp(statement->statement_name, "sub") == 0 ||
            strcmp(statement->statement_name, "stack_addr") == 0)
        {
            assert(statement->args[0].variant == OP_KIND_VALUE);
            statement->output = make_value(statement->args[0].value->type);
            statement->output->variant = VALUE_SSA;
        }
        else
            assert(("TODO", 0));
    }
}

static void connect_statement_to_operand(Statement * statement, Operand op)
{
    if (op.variant == OP_KIND_VALUE)
    {
        Value * val = op.value;
        
        if (val->variant == VALUE_ARG || val->variant == VALUE_SSA || val->variant == VALUE_STACKADDR)
            array_push(val->edges_out, Statement *, statement);
    }
}

#endif // BBAE_COMPILER_COMMON
