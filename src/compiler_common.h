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
static char * find_next_token(const char ** b)
{
    static char token[4096];
    static size_t token_len;
    
    memset(token, 0, 4096);
    token_len = 0;
    
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
    if (is_comment(*b))
    {
        while (!is_newline(**b) && **b != 0)
            *b += 1;
    }
    
    assert(token[4095] == 0);
    
    if (token[0] != 0)
        return token;
    else
        return 0;
}
// find the next token even if it's on a different line
// thrashes any previously-returned token
static char * find_next_token_anywhere(const char ** b)
{
    while ((is_newline(**b) || is_space(**b) || is_comment(*b)) && **b != 0)
    {
        if (is_comment(*b))
        {
            while (!is_newline(**b) && **b != 0)
                *b += 1;
        }
        *b += 1;
    }
    
    return find_next_token(b);
}
// TODO: emit a warning or error if other tokens are encountered
static void find_end_of_line(const char ** b)
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
        str += 1;
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
    TYPE_IPTR,
    TYPE_F32,
    TYPE_F64,
    TYPE_AGG,
};

typedef struct _AggData {
    size_t align;
    size_t size;
    uint8_t * per_byte_likeness; // pointer to an array of uint8_ts with size `size`. 0 = int, 1 = float
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

static uint8_t type_is_valid(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_AGG;
}
static uint8_t type_is_basic(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_F64;
}
static uint8_t type_is_agg(Type type)
{
    return type.variant == TYPE_AGG;
}
static uint8_t type_is_float_only_agg(Type type)
{
    if (type_is_agg(type))
    {
        for (size_t i = 0; i < type.aggdata.size; i++)
        {
            if (!type.aggdata.per_byte_likeness[i])
                return 0;
        }
        return 1;
    }
    return 0;
}
static uint8_t type_is_int(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_I64;
}
static uint8_t type_is_float(Type type)
{
    return type.variant >= TYPE_F32 && type.variant <= TYPE_F64;
}
static uint8_t type_is_ptr(Type type)
{
    return type.variant == TYPE_IPTR;
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
    else if (type.variant == TYPE_IPTR)
        return 8;
    else if (type.variant == TYPE_F32)
        return 4;
    else if (type.variant == TYPE_F64)
        return 8;
    else
        assert(0);
}

uint8_t aggdata_same(AggData a, AggData b)
{
    if (a.align != b.align || a.packed != b.packed || a.size != b.size)
        return 0;
    return !memcmp(a.per_byte_likeness, b.per_byte_likeness, a.size);
}

uint8_t types_same(Type a, Type b)
{
    if (a.variant == TYPE_INVALID || b.variant == TYPE_INVALID)
        return 0;
    if (a.variant != b.variant)
        return 0;
    if (a.variant != TYPE_AGG)
        return 1;
    return aggdata_same(a.aggdata, b.aggdata);
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
    const char * arg;
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
    
    uint64_t temp; // temporary, used by specific algorithms as a kind of cache
} Value;

enum BBAE_OP_VARIANT {
    OP_KIND_INVALID,
    OP_KIND_TYPE,
    OP_KIND_VALUE,
    OP_KIND_TEXT,
    OP_KIND_SEPARATOR,
};

typedef struct _Operand {
    enum BBAE_OP_VARIANT variant;
    Type rawtype;
    Value * value;
    const char * text;
} Operand;

struct _Block;
typedef struct _Statement {
    const char * output_name; // if null, instruction. else, operation.
    Value * output; // if null, instruction. else, operation.
    const char * statement_name;
    struct _Block * block;
    
    uint64_t num; // only used during register allocation; zero until then
    
    // array
    Operand * args;
} Statement;

static Statement * new_statement(void)
{
    Statement * statement = (Statement *)zero_alloc(sizeof(Statement));
    statement->args = (Operand *)zero_alloc(0);
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
    // where the block starts within its associated byte buffer
    uint64_t start_offset;
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
    uint64_t stack_height;
    // blocks, in order of declaration (array)
    Block ** blocks;
    // explicit pointer to entry block so that it doesn't get lost
    Block * entry_block;
    
    uint8_t written_registers[256]; // list of registers that have been written to in the function. used to avoid clobbering callee-saved registers.
} Function;

static Function * new_func(void)
{
    Function * func = (Function *)zero_alloc(sizeof(Function));
    func->args = (Value **)zero_alloc(0);
    func->stack_slots = (Value **)zero_alloc(0);
    func->blocks = (Block **)zero_alloc(0);
    return func;
}

static Block * find_block(Function * func, const char * name)
{
    for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
    {
        Block * block = func->blocks[b];
        if (strcmp(block->name, name) == 0)
            return block;
    }
    return 0;
}

typedef struct _StaticData {
    const char * name;
    Type type;
    uint64_t init_data_short; // only used if size of type is 8 or less
    uint8_t * init_data_long;
    size_t location; // starts out uninitialized, becomes initialized when code generation is done
} StaticData;

typedef struct _Program {
    // array
    Function ** functions;
    //Global * globals;
    StaticData * statics;
    Function * current_func;
    Block * current_block;
} Program;

static void add_static_i8(Program * program, const char * name, uint8_t value)
{
    StaticData data = {name, basic_type(TYPE_I8), value, 0, 0};
    array_push(program->statics, StaticData, data);
}
static void add_static_i16(Program * program, const char * name, uint16_t value)
{
    StaticData data = {name, basic_type(TYPE_I16), value, 0, 0};
    array_push(program->statics, StaticData, data);
}
static void add_static_i32(Program * program, const char * name, uint32_t value)
{
    StaticData data = {name, basic_type(TYPE_I32), value, 0, 0};
    array_push(program->statics, StaticData, data);
}
static void add_static_i64(Program * program, const char * name, uint64_t value)
{
    StaticData data = {name, basic_type(TYPE_I64), value, 0, 0};
    array_push(program->statics, StaticData, data);
}

static StaticData find_static(Program * program, const char * name)
{
    for (size_t i = 0; i < array_len(program->statics, StaticData); i++)
    {
        StaticData stat = program->statics[i];
        if (strcmp(stat.name, name) == 0)
            return stat;
    }
    StaticData ret;
    memset(&ret, 0, sizeof(StaticData));
    return ret;
}

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

static Type parse_type(const char ** b)
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
    else if (strcmp(token, "iptr") == 0)
        type.variant = TYPE_IPTR;
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
    Value * ret = make_value(basic_type(TYPE_IPTR));
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

static Operand new_op_type(Type rawtype)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_TYPE;
    op.rawtype = rawtype;
    return op;
}

static Operand new_op_text(const char * name)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_TEXT;
    op.text = name;
    return op;
}

static Operand new_op_separator(void)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_SEPARATOR;
    return op;
}

static uint64_t temp_ctr = 0;
static char * make_temp_name(void)
{
    temp_ctr += 1;
    size_t len = snprintf(0, 0, "__bbae_temp_%zu", temp_ctr) + 1;
    char * str = zero_alloc(len + 1);
    snprintf(str, len, "__bbae_temp_%zu", temp_ctr);
    assert(len > 12);
    assert(strcmp(str, "__bbae_temp_") != 0);
    return str;
}

static void add_statement_output(Statement * statement)
{
    if (statement->output_name)
    {
        if (strcmp(statement->statement_name, "add") == 0 ||
            strcmp(statement->statement_name, "sub") == 0 ||
            strcmp(statement->statement_name, "mul") == 0 ||
            strcmp(statement->statement_name, "div") == 0 ||
            strcmp(statement->statement_name, "shl") == 0 ||
            strcmp(statement->statement_name, "shr") == 0 ||
            strcmp(statement->statement_name, "fadd") == 0 ||
            strcmp(statement->statement_name, "fsub") == 0 ||
            strcmp(statement->statement_name, "fmul") == 0 ||
            strcmp(statement->statement_name, "fdiv") == 0 ||
            strcmp(statement->statement_name, "fxor") == 0 ||
            strcmp(statement->statement_name, "mov") == 0)
        {
            assert(statement->args[0].variant == OP_KIND_VALUE);
            statement->output = make_value(statement->args[0].value->type);
        }
        else if (strcmp(statement->statement_name, "load") == 0 ||
                 strcmp(statement->statement_name, "uint_to_float") == 0 ||
                 strcmp(statement->statement_name, "bitcast") == 0)
        {
            assert(statement->args[0].variant == OP_KIND_TYPE);
            statement->output = make_value(statement->args[0].rawtype);
        }
        else if (strcmp(statement->statement_name, "cmp_g") == 0 ||
                 strcmp(statement->statement_name, "cmp_ge") == 0 ||
                 strcmp(statement->statement_name, "cmp_ge") == 0)
        {
            statement->output = make_value(basic_type(TYPE_I8));
        }
        else
        {
            printf("culprit: %s\n", statement->statement_name);
            assert(("TODO", 0));
        }
        statement->output->variant = VALUE_SSA;
        statement->output->ssa = statement;
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

static void disconnect_statement_from_operand(Statement * statement, Operand op)
{
    if (op.variant != OP_KIND_VALUE)
        return;
    Value * val = op.value;
    for (size_t i = 0; i < array_len(val->edges_out, Statement *); i++)
    {
        if (val->edges_out[i] == statement)
        {
            array_erase(val->edges_out, Statement *, i);
            break;
        }
    }
}

size_t find_separator_index(Operand * args)
{
    for (size_t i = 2; i < array_len(args, Operand); i++)
    {
        if (args[i].variant == OP_KIND_SEPARATOR)
            return i;
    }
    return 0;
}

// On some backends, specific types of argument can't be used with specific functions.
// For example, on x86, there's no <reg_a> = fmul <reg_a> <const> instruction.
// So, we need a legalization system. Our legalization system runs immediately before an instruction
// is regalloced

typedef struct _ImmOpsAllowed
{
    // For if and goto instructions, all operands are legal (except for the ones that need to be label names).
    // For branches, it's the backend's job to emit all the relevant machine code.
    // The backend knows exactly which registers are allowed to be clobbered during jumps, so it's able to do so.
    
    uint8_t immediates_allowed[8];
} ImmOpsAllowed;
// implemented by backend
ImmOpsAllowed imm_op_rule_determiner(Statement * statement);

typedef struct _RegAllocRules
{
    // for up to 8 operands (0 is output, 1 is input 0, 2 is input 1, etc):
    // which registers from 0 to 63 are allowed
    uint64_t allowed_registers_lo[8];
    // 64 to 127, etc. not used in x86 backend, but useful in exotic architectures.
    // might also be useful for x86 in the future for extended SIMD support? dunno
    uint64_t allowed_registers_e1[8];
    uint64_t allowed_registers_e2[8];
    uint64_t allowed_registers_e3[8];
    
    // which registers the instruction clobbers, if any
    uint8_t clobbers[256];
    
    uint8_t output_same_as_left_input; // for x86, almost always 1
    uint8_t raw_stack_addr_arguments_allowed;
} RegAllocRules;
// implemented by backend
RegAllocRules regalloc_rule_determiner(Statement * statement);


    
#endif // BBAE_COMPILER_COMMON
