#ifndef BBAE_COMPILER_COMMON
#define BBAE_COMPILER_COMMON

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"

// On some backends, specific types of argument can't be used with specific functions.
// For example, on x86, there's no <reg_a> = fmul <reg_a> <const> instruction.
// So, we need to legalize literals/constants. Our const legalization runs immediately before an instruction is regalloced.
typedef struct _ImmOpsAllowed
{
    uint8_t immediates_allowed[8];
} ImmOpsAllowed;
// implemented by backend
struct _Statement;
static inline ImmOpsAllowed imm_op_rule_determiner(struct _Statement * statement);

typedef struct _SymbolEntry
{
    const char * name;
    size_t loc;
    uint8_t kind; // 0 = invalid; 1 = function; 2 = static inline data; 3 = global variable (different address space)
    uint8_t visibility; // 0 = private, 1 = default (export), 2 = dllexport
} SymbolEntry;

typedef struct _UnknownSymbolUsage
{
    const char * name;
    size_t loc;
} UnknownSymbolUsage;

typedef struct _CompilerMetaOutput
{
    // array
    UnknownSymbolUsage * unknown_symbol_usages;
    // array
    SymbolEntry * symbols;
    size_t global_var_bytes;
} CompilerMetaOutput;


// read the next token on the current line, return 0 if none
// thrashes any previously-returned token. make copies!
static inline char * find_next_token(const char ** b)
{
    //printf("starting character... %02X\n", (uint8_t)**b);
    
    static char token[4096];
    static size_t token_len;
    
    memset(token, 0, 4096);
    token_len = 0;
    
    char * w = token;
    
    // skip leading spaces
    while (**b != 0 && is_space(**b))
    {
        *b += 1;
        if (**b == 0)
            return 0;
    }
    
    while (**b != 0 && !is_newline(**b) && !is_space(**b) && token_len < 4096 && !is_comment(*b))
    {
        *w = **b;
        w += 1;
        *b += 1;
        token_len += 1;
    }
    if (is_comment(*b))
    {
        while (**b != 0 && !is_newline(**b))
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
static inline char * find_next_token_anywhere(const char ** b)
{
    //printf("anywhere starting character... %02X\n", (uint8_t)**b);
    
    if (b == 0 || *b == 0 || **b == 0)
        return 0;
    
    while (**b != 0 && (is_newline(**b) || is_space(**b) || is_comment(*b)))
    {
        if (is_comment(*b))
        {
            while (**b != 0 && !is_newline(**b))
                *b += 1;
        }
        if (**b != 0)
            *b += 1;
    }
    
    return find_next_token(b);
}
// TODO: emit a warning or error if other tokens are encountered
static inline void find_end_of_line(const char ** b)
{
    while (!is_newline(**b) && **b != 0)
        *b += 1;
}

static inline uint64_t size_guess_align(uint64_t s)
{
    uint64_t ret = 1;
    while (ret < 64 && ret < s)
        ret <<= 1;
    return ret;
}

static inline int char_value(const char c)
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
static inline uint64_t my_strtoull(const char * str, const char ** endptr)
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
    
    return negative ? (uint64_t)-(int64_t)out : out;
}

static uint64_t temp_ctr = 0;
static inline char * make_temp_name(void)
{
    temp_ctr += 1;
    //size_t len = snprintf(0, 0, "__bbae_temp_%zu", temp_ctr) + 1;
    size_t len = snprintf(0, 0, "btemp_%zu", temp_ctr) + 1;
    char * str = (char *)zero_alloc(len + 1);
    //snprintf(str, len, "__bbae_temp_%zu", temp_ctr);
    snprintf(str, len, "btemp_%zu", temp_ctr);
    //assert(strcmp(str, "__bbae_temp_") != 0);
    assert(strcmp(str, "btemp_") != 0);
    return str;
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

static inline Type basic_type(enum BBAE_TYPE_VARIANT val)
{
    Type ret;
    memset(&ret, 0, sizeof(Type));
    ret.variant = val;
    return ret;
}

const char * type_to_static_string(Type type)
{
    if (type.variant == TYPE_NONE)
        return "void";
    if (type.variant == TYPE_I8)
        return "i8";
    if (type.variant == TYPE_I16)
        return "i16";
    if (type.variant == TYPE_I32)
        return "i32";
    if (type.variant == TYPE_I64)
        return "i64";
    if (type.variant == TYPE_F32)
        return "f32";
    if (type.variant == TYPE_F64)
        return "f64";
    if (type.variant == TYPE_IPTR)
        return "iptr";
    if (type.variant == TYPE_AGG)
        return "<aggregatetype>";
    if (type.variant == TYPE_INVALID)
        return "<INVALIDTYPE>";
    return "<BROKENTYPE>";
}

static inline uint8_t type_is_valid(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_AGG;
}
static inline uint8_t type_is_basic(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_F64;
}
static inline uint8_t type_is_agg(Type type)
{
    return type.variant == TYPE_AGG;
}
static inline uint8_t type_is_float_only_agg(Type type)
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
static inline uint8_t type_is_int(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_I64;
}
static inline uint8_t type_is_float(Type type)
{
    return type.variant >= TYPE_F32 && type.variant <= TYPE_F64;
}
static inline uint8_t type_is_ptr(Type type)
{
    return type.variant == TYPE_IPTR;
}

static inline size_t type_size(Type type)
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
    {
        assert(0);
        return 0; // silence broken MSVC warning
    }
}

static inline uint8_t aggdata_same(AggData a, AggData b)
{
    if (a.align != b.align || a.packed != b.packed || a.size != b.size)
        return 0;
    return !memcmp(a.per_byte_likeness, b.per_byte_likeness, a.size);
}

static inline uint8_t types_same(Type a, Type b)
{
    if (a.variant == TYPE_INVALID || b.variant == TYPE_INVALID)
        return 0;
    if (a.variant != b.variant)
        return 0;
    if (a.variant != TYPE_AGG)
        return 1;
    return aggdata_same(a.aggdata, b.aggdata);
}

typedef struct _GlobalData {
    const char * name;
    Type type;
    size_t location;
    size_t allocated_size;
    uint8_t is_private;
} GlobalData;

enum BBAE_VALUE_VARIANT {
    VALUE_INVALID,
    VALUE_CONST,
    VALUE_SSA,
    VALUE_ARG,
    VALUE_STACKADDR,
};

struct _StackSlot;
typedef struct _Value {
    enum BBAE_VALUE_VARIANT variant; // values can be constants, SSA values produced by operations, or func/block arguments
    Type type;
    uint64_t constant; // is a pointer if type is aggregate. otherwise is bits of value
    struct _Statement * ssa;
    const char * arg;
    struct _StackSlot * slotinfo;
    
    // array
    struct _Statement ** edges_out;
    
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

static inline uint8_t value_is_basic_zero_constant(Value * value)
{
    return value->variant == VALUE_CONST && type_is_basic(value->type) && value->constant == 0;
}

enum BBAE_OP_VARIANT {
    OP_KIND_INVALID,
    OP_KIND_TYPE,
    OP_KIND_VALUE,
    OP_KIND_TEXT,
    OP_KIND_RAWINTEGER,
    OP_KIND_SEPARATOR,
};

typedef struct _Operand {
    enum BBAE_OP_VARIANT variant;
    Type rawtype;
    Value * value;
    const char * text;
    int64_t rawint;
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
    
    uint64_t temp; // temporary, used by specific algorithms as a kind of cache
} Statement;

static inline Statement * new_statement(void)
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
    // statements within the block (array)
    Statement ** statements;
    // where the block starts within its associated byte buffer
    uint64_t start_offset;
} Block;

static inline Block * new_block(void)
{
    Block * block = (Block *)zero_alloc(sizeof(Block));
    block->args = (Value **)zero_alloc(0);
    block->edges_in = (Statement **)zero_alloc(0);
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
    
    // metadata used by some optimizations
    size_t statement_count; // inlining heuristic
    uint8_t performs_calls; // inlining heuristic and regalloc heuristic
} Function;

static inline Function * new_func(void)
{
    Function * func = (Function *)zero_alloc(sizeof(Function));
    func->args = (Value **)zero_alloc(0);
    func->stack_slots = (Value **)zero_alloc(0);
    func->blocks = (Block **)zero_alloc(0);
    return func;
}

static inline Block * find_block(Function * func, const char * name)
{
    if (name == 0)
        return 0;
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
    uint8_t is_private;
} StaticData;

typedef struct _NameUsageInfo
{
    uint64_t loc;
    const char * name;
    uint8_t size;
    // TODO: whether the relocation is end-relative, start-relative, or absolute (currently only end-relative)
} NameUsageInfo;

typedef struct _UnusedRelocation
{
    NameUsageInfo info;
} UnusedRelocation;


typedef struct _Program {
    // arrays
    Function ** functions;
    GlobalData * globals;
    size_t globals_bytecount; // number of bytes for globals, not number of globals
    StaticData * statics;
    UnusedRelocation * unused_relocation_log;
    // non-arrays
    Function * current_func;
    Block * current_block;
    
    uint8_t construction_finished;
} Program;

static inline Function * find_func(Program * program, const char * name)
{
    if (name == 0)
        return 0;
    for (size_t i = 0; i < array_len(program->functions, Function *); i++)
    {
        if (strcmp(program->functions[i]->name, name) == 0)
            return program->functions[i];
    }
    return 0;
}

static inline const char * find_static_by_value(Program * program, Type type, uint64_t value, uint8_t private_only)
{
    for (size_t i = 0; i < array_len(program->statics, StaticData); i++)
    {
        StaticData stat = program->statics[i];
        if (!private_only && !stat.is_private)
            continue;
        if (!types_same(stat.type, type))
            continue;
        if (type_is_agg(stat.type))
        {
            uint8_t * data = (uint8_t *)value;
            if (memcmp(data, stat.init_data_long, type_size(stat.type)) == 0)
                return stat.name;
        }
        else if (stat.init_data_short == value)
            return stat.name;
    }
    return 0;
}

static inline GlobalData add_global(Program * program, const char * name, Type type_, uint8_t is_private)
{
    size_t allocated_size = type_size(type_);
    size_t align = size_guess_align(allocated_size);
    
    //while (allocated_size % align)
    //    allocated_size += 1;
    
    while (program->globals_bytecount % align)
        program->globals_bytecount += 1;
    
    GlobalData data = {name, type_, program->globals_bytecount, allocated_size, is_private};
    
    program->globals_bytecount += allocated_size;
    
    array_push(program->globals, GlobalData, data);
    return data;
}

static inline GlobalData find_global(Program * program, const char * name)
{
    for (size_t i = 0; i < array_len(program->globals, GlobalData); i++)
    {
        GlobalData stat = program->globals[i];
        if (strcmp(stat.name, name) == 0)
            return stat;
    }
    GlobalData ret;
    memset(&ret, 0, sizeof(GlobalData));
    return ret;
}

static inline void add_static_i8(Program * program, const char * name, uint8_t value, uint8_t is_private)
{
    StaticData data = {name, basic_type(TYPE_I8), value, 0, 0, is_private};
    array_push(program->statics, StaticData, data);
}
static inline const char * add_static_i8_anonymous(Program * program, uint8_t value)
{
    const char * found_name = find_static_by_value(program, basic_type(TYPE_I8), value, 1);
    if (found_name)
        return found_name;
    const char * name = make_temp_name();
    add_static_i8(program, name, value, 1);
    return name;
}
static inline void add_static_i16(Program * program, const char * name, uint16_t value, uint8_t is_private)
{
    StaticData data = {name, basic_type(TYPE_I16), value, 0, 0, is_private};
    array_push(program->statics, StaticData, data);
}
static inline const char * add_static_i16_anonymous(Program * program, uint16_t value)
{
    const char * found_name = find_static_by_value(program, basic_type(TYPE_I16), value, 1);
    if (found_name)
        return found_name;
    const char * name = make_temp_name();
    add_static_i16(program, name, value, 1);
    return name;
}
static inline void add_static_i32(Program * program, const char * name, uint32_t value, uint8_t is_private)
{
    StaticData data = {name, basic_type(TYPE_I32), value, 0, 0, is_private};
    array_push(program->statics, StaticData, data);
}
static inline const char * add_static_i32_anonymous(Program * program, uint32_t value)
{
    const char * found_name = find_static_by_value(program, basic_type(TYPE_I32), value, 1);
    if (found_name)
        return found_name;
    const char * name = make_temp_name();
    add_static_i32(program, name, value, 1);
    return name;
}
static inline void add_static_i64(Program * program, const char * name, uint64_t value, uint8_t is_private)
{
    StaticData data = {name, basic_type(TYPE_I64), value, 0, 0, is_private};
    array_push(program->statics, StaticData, data);
}
static inline const char * add_static_i64_anonymous(Program * program, uint64_t value)
{
    const char * found_name = find_static_by_value(program, basic_type(TYPE_I64), value, 1);
    if (found_name)
        return found_name;
    const char * name = make_temp_name();
    add_static_i64(program, name, value, 1);
    return name;
}

static inline StaticData find_static(Program * program, const char * name)
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

static inline uint64_t parse_int_bare(const char * n)
{
    size_t len = strlen(n);
    
    const char * end = 0;
    uint64_t ret = (uint64_t)my_strtoull(n, &end);
    assert(((void)"bare integer literal has trailing characters", (uint64_t)(end - n) == len));
    
    return ret;
}

static inline uint64_t parse_int_nonbare(const char * n)
{
    const char * end = 0;
    uint64_t ret = (uint64_t)my_strtoull(n, &end);
    return ret;
}

static inline Type parse_type(const char ** b)
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
        //uint8_t is_packed = 0;
        token = find_next_token(b);
        if (strcmp(token, "packed") == 0)
        {
            //is_packed = 1;
            token = find_next_token(b);
        }
        if (str_begins_with(token, "align."))
        {
            uint64_t align = parse_int_bare(token + 6);
            align = align + 0; // suppress unused variable warning
            token = find_next_token(b);
        }
        else
            assert(((void)"missing alignment in aggregate type spec", 0));
        
        assert(((void)"TODO: properly parse struct types", 0));
    }
    else
    {
        assert(((void)"invalid type", 0));
    }
    
    return type;
}

static inline Value * make_value(Type type)
{
    Value * ret = (Value *)zero_alloc(sizeof(Value));
    ret->edges_out = (Statement **)zero_alloc(0);
    ret->type = type;
    return ret;
}

static inline Value * make_const_value(enum BBAE_TYPE_VARIANT variant, uint64_t data)
{
    Value * ret = make_value(basic_type(variant));
    ret->variant = VALUE_CONST;
    ret->constant = data;
    return ret;
}

static inline Value * make_stackslot_value(StackSlot * slot)
{
    Value * ret = make_value(basic_type(TYPE_IPTR));
    ret->variant = VALUE_STACKADDR;
    ret->slotinfo = slot;
    return ret;
}


static inline Operand new_op_rawint(uint64_t rawint)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_RAWINTEGER;
    op.rawint = rawint;
    return op;
}

static inline Operand new_op_val(Value * val)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_VALUE;
    op.value = val;
    return op;
}

static inline Operand new_op_type(Type rawtype)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_TYPE;
    op.rawtype = rawtype;
    return op;
}

static inline Operand new_op_text(const char * name)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_TEXT;
    op.text = name;
    return op;
}

static inline Operand new_op_separator(void)
{
    Operand op;
    memset(&op, 0, sizeof(Operand));
    op.variant = OP_KIND_SEPARATOR;
    return op;
}

static inline void add_statement_output(Statement * statement)
{
    if (statement->output_name)
    {
        if (strcmp(statement->statement_name, "add") == 0 ||
            strcmp(statement->statement_name, "sub") == 0 ||
            strcmp(statement->statement_name, "mul") == 0 ||
            strcmp(statement->statement_name, "imul") == 0 ||
            strcmp(statement->statement_name, "div") == 0 ||
            strcmp(statement->statement_name, "div_unsafe") == 0 ||
            strcmp(statement->statement_name, "idiv") == 0 ||
            strcmp(statement->statement_name, "idiv_unsafe") == 0 ||
            strcmp(statement->statement_name, "rem") == 0 ||
            strcmp(statement->statement_name, "rem_unsafe") == 0 ||
            strcmp(statement->statement_name, "irem") == 0 ||
            strcmp(statement->statement_name, "irem_unsafe") == 0 ||
            strcmp(statement->statement_name, "shl") == 0 ||
            strcmp(statement->statement_name, "shr") == 0 ||
            strcmp(statement->statement_name, "shr_unsafe") == 0 ||
            strcmp(statement->statement_name, "sar") == 0 ||
            strcmp(statement->statement_name, "sar_unsafe") == 0 ||
            strcmp(statement->statement_name, "fadd") == 0 ||
            strcmp(statement->statement_name, "fsub") == 0 ||
            strcmp(statement->statement_name, "fmul") == 0 ||
            strcmp(statement->statement_name, "fdiv") == 0 ||
            strcmp(statement->statement_name, "fxor") == 0 ||
            
            strcmp(statement->statement_name, "and") == 0 ||
            strcmp(statement->statement_name, "or") == 0 ||
            strcmp(statement->statement_name, "xor") == 0 ||
            
            strcmp(statement->statement_name, "mov") == 0
            )
        {
            assert(statement->args[0].variant == OP_KIND_VALUE);
            statement->output = make_value(statement->args[0].value->type);
        }
        else if (strcmp(statement->statement_name, "load") == 0 ||
                 // int casts
                 strcmp(statement->statement_name, "trim") == 0 ||
                 strcmp(statement->statement_name, "qext") == 0 ||
                 strcmp(statement->statement_name, "sext") == 0 ||
                 strcmp(statement->statement_name, "zext") == 0 ||
                 // float-int casts
                 strcmp(statement->statement_name, "float_to_uint") == 0 ||
                 strcmp(statement->statement_name, "float_to_uint_unsafe") == 0 ||
                 strcmp(statement->statement_name, "uint_to_float") == 0 ||
                 strcmp(statement->statement_name, "float_to_sint") == 0 ||
                 strcmp(statement->statement_name, "float_to_sint_unsafe") == 0 ||
                 strcmp(statement->statement_name, "sint_to_float") == 0 ||
                 
                 strcmp(statement->statement_name, "bitcast") == 0 ||
                 // non-casts
                 strcmp(statement->statement_name, "extract") == 0
                 )
        {
            assert(statement->args[0].variant == OP_KIND_TYPE);
            statement->output = make_value(statement->args[0].rawtype);
        }
        else if (strcmp(statement->statement_name, "cmp_g" ) == 0 ||
                 strcmp(statement->statement_name, "cmp_ge") == 0 ||
                 strcmp(statement->statement_name, "cmp_l" ) == 0 ||
                 strcmp(statement->statement_name, "cmp_le") == 0 ||
                 strcmp(statement->statement_name, "cmp_eq") == 0 ||
                 strcmp(statement->statement_name, "cmp_ne") == 0 ||
                 strcmp(statement->statement_name, "icmp_g" ) == 0 ||
                 strcmp(statement->statement_name, "icmp_ge") == 0 ||
                 strcmp(statement->statement_name, "icmp_l" ) == 0 ||
                 strcmp(statement->statement_name, "icmp_le") == 0 ||
                 strcmp(statement->statement_name, "fcmp_g" ) == 0 ||
                 strcmp(statement->statement_name, "fcmp_ge") == 0 ||
                 strcmp(statement->statement_name, "fcmp_l" ) == 0 ||
                 strcmp(statement->statement_name, "fcmp_le") == 0 ||
                 strcmp(statement->statement_name, "fcmp_eq") == 0 ||
                 strcmp(statement->statement_name, "fcmp_ne") == 0)
        {
            statement->output = make_value(basic_type(TYPE_I8));
        }
        else if (strcmp(statement->statement_name, "symbol_lookup_unsized") == 0 ||
                 strcmp(statement->statement_name, "symbol_lookup") == 0)
        {
            statement->output = make_value(basic_type(TYPE_IPTR));
        }
        else if (strcmp(statement->statement_name, "call_eval") == 0 ||
                 strcmp(statement->statement_name, "call") == 0)
        {
            Type type = statement->args[0].rawtype;
            array_erase(statement->args, Operand, 0);
            statement->output = make_value(type);
        }
        else
        {
            printf("culprit: %s\n", statement->statement_name);
            assert(((void)"TODO", 0));
        }
        statement->output->variant = VALUE_SSA;
        statement->output->ssa = statement;
    }
}

static inline uint8_t values_same(Value * a, Value * b)
{
    if (a == b)
        return 1;
    if (a->variant != b->variant)
        return 0;
    if (a->variant == VALUE_CONST)
    {
        if (!types_same(a->type, b->type))
            return 0;
        if (type_is_agg(a->type))
            return memcmp((void *)a->constant, (void *)b->constant, type_size(a->type)) == 0;
        else
            return a->constant == b->constant;
    }
    if (a->variant == VALUE_ARG)
        return strcmp(a->arg, b->arg) == 0;
    
    return memcmp(a, b, sizeof(Value)) == 0;
}

static inline uint8_t operands_same(Operand a, Operand b)
{
    // different kind of operand
    if (a.variant != b.variant)
        return 0;
    // invalid operands are never the same
    if (a.variant == OP_KIND_INVALID)
        return 0;
    // separators have no info, and are always the same
    else if (a.variant == OP_KIND_SEPARATOR)
        return 1;
    else if (a.variant == OP_KIND_TYPE)
        return types_same(a.rawtype, b.rawtype);
    else if (a.variant == OP_KIND_TEXT)
        return strcmp(a.text, b.text) == 0;
    else if (a.variant == OP_KIND_VALUE)
        return values_same(a.value, b.value);
    else if (a.variant == OP_KIND_RAWINTEGER)
        return a.rawint == b.rawint;
    else 
    {
        assert("FIXME" && 0);
        return 0; // silence broken MSVC warning
    }
}

static inline void connect_statement_to_operand(Statement * statement, Operand op)
{
    uint8_t found = 0;
    for (size_t i = 0; i < array_len(statement->args, Operand); i++)
    {
        if (operands_same(statement->args[i], op))
        {
            found = 1;
            break;
        }
    }
    assert(found);
    Value * val = op.value;
    if (val && (val->variant == VALUE_ARG || val->variant == VALUE_SSA || val->variant == VALUE_STACKADDR))
        array_push(val->edges_out, Statement *, statement);
}

static inline void disconnect_statement_from_operand(Statement * statement, Operand op, uint8_t once)
{
    if (op.variant != OP_KIND_VALUE)
        return;
    Value * val = op.value;
    for (size_t i = 0; i < array_len(val->edges_out, Statement *); i++)
    {
        if (val->edges_out[i] == statement)
        {
            array_erase(val->edges_out, Statement *, i);
            i -= 1;
            if (once)
                break;
        }
    }
}

static inline size_t find_separator_index(Operand * args)
{
    for (size_t i = 2; i < array_len(args, Operand); i++)
    {
        if (args[i].variant == OP_KIND_SEPARATOR)
            return i;
    }
    return (size_t)-1;
}

static inline uint8_t statement_is_terminator(Statement * a)
{
    if (!a)
        return 0;
    if (strcmp(a->statement_name, "goto") == 0 ||
        strcmp(a->statement_name, "if") == 0 ||
        strcmp(a->statement_name, "exit") == 0 ||
        strcmp(a->statement_name, "return") == 0)
        return 1;
    return 0;
}

static inline uint8_t statement_has_side_effects(Statement * a)
{
    assert(a);
    // statements with no output always have side effects
    if (!a->output)
        return 1;
    // terminators always have side effects (on control flow)
    if (statement_is_terminator(a))
        return 1;
    // function calls always have side effects
    if (strcmp(a->statement_name, "call_eval") == 0)
        return 1;
    // FIXME check for volatile loads
    return 0;
}

static inline uint8_t statements_same(Statement * a, Statement * b)
{
    // "same" for statements here means that they can safely be substituted for one another or combined.
    
    // exact same object
    if (a == b)
        return 1;
    // output-having-ness is different
    if (!!a->output != !!b->output)
        return 0;
    // output types are different
    if (a->output && !types_same(a->output->type, b->output->type))
        return 0;
    // arg count is different
    if (array_len(a->args, Operand) != array_len(b->args, Operand))
        return 0;
    // statement type is different
    if (strcmp(a->statement_name, b->statement_name) != 0)
        return 0;
    // strictly speaking, statements with side effects are *never* the same in SSA terms, because they can't be combined.
    if (statement_has_side_effects(a) || statement_has_side_effects(b))
        return 0;
    // same output type, same statement name, same argument count... we need to check the individual operands.
    for (size_t i = 0; i < array_len(a->args, Operand); i++)
    {
        if (!operands_same(a->args[i], b->args[i]))
            return 0;
    }
    // every operand is the same, so we'll say the statements are the same.
    return 1;
}

static inline void block_replace_statement_val_args(Block * block, Value * old, Value * new_val)
{
    for (size_t j = 0; j < array_len(block->statements, Statement *); j++)
    {
        Statement * statement = block->statements[j];
        for (size_t n = 0; n < array_len(statement->args, Operand); n++)
        {
            if (statement->args[n].variant == OP_KIND_VALUE && statement->args[n].value == old)
            {
                disconnect_statement_from_operand(statement, statement->args[n], 1);
                statement->args[n].value = new_val;
                connect_statement_to_operand(statement, statement->args[n]);
            }
        }
    }
}

static inline void validate_links(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                assert(statement->block == block);
                if (strcmp(statement->statement_name, "goto") == 0)
                {
                    assert(array_len(statement->args, Value *) > 0);
                    assert(statement->args[0].variant == OP_KIND_TEXT);
                    for (size_t i = 1; i < array_len(statement->args, Operand); i++)
                    {
                        Operand op = statement->args[i];
                        assert(op.variant == OP_KIND_VALUE);
                    }
                    Block * next = find_block(func, statement->args[0].text);
                    assert(next);
                    size_t block_arg_count = array_len(next->args, Value *);
                    size_t statement_arg_count = array_len(statement->args, Operand);
                    if (block_arg_count != statement_arg_count - 1)
                    {
                        printf("%zu %zu\n", block_arg_count, statement_arg_count - 1);
                        assert(block_arg_count == statement_arg_count - 1);
                    }
                }
                if (statement->output)
                {
                    //size_t edge_count = array_len(statement->output->edges_out, Statement *);
                    for (size_t e = 0; e < array_len(statement->output->edges_out, Statement *); e++)
                    {
                        Statement * other = statement->output->edges_out[e];
                        assert(other->block == block);
                        uint8_t found_arg = 0;
                        for (size_t i = 0; i < array_len(other->args, Operand); i++)
                        {
                            if (other->args[i].value == statement->output)
                            {
                                found_arg = 1;
                                break;
                            }
                        }
                        assert(found_arg);
                        
                        uint8_t found_statement = 0;
                        for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
                        {
                            if (block->statements[i] == other)
                            {
                                found_statement = 1;
                                break;
                            }
                        }
                        assert(found_statement);
                    }
                }
                for (size_t a = 0; a < array_len(statement->args, Operand); a++)
                {
                    Operand op = statement->args[a];
                    if (op.value)
                    {
                        if (op.value->ssa)
                            assert(op.value->ssa->block == block);
                        else if (op.value->arg)
                        {
                            uint8_t found_arg_used_by_statement = 0;
                            size_t ba_count = (b != 0) ? array_len(block->args, Value *) : array_len(func->args, Value *);
                            for (size_t ba = 0; ba < ba_count; ba++)
                            {
                                Value * block_arg = (b != 0) ? block->args[ba] : func->args[ba];
                                if (strcmp(block_arg->arg, op.value->arg) == 0)
                                {
                                    found_arg_used_by_statement = 1;
                                    break;
                                }
                            }
                            if (!found_arg_used_by_statement)
                            {
                                printf("%s\n", op.value->arg);
                                assert(found_arg_used_by_statement);
                            }
                        }
                    }
                }
            }
        }
    }
}

static inline void verify_coherency(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            assert(array_len(block->statements, Statement *) >= 1);
            for (size_t i = 0; i < array_len(block->statements, Statement *) - 1; i++)
            {
                Statement * statement = block->statements[i];
                assert(strcmp(statement->statement_name, "if") != 0);
                assert(strcmp(statement->statement_name, "goto") != 0);
                assert(strcmp(statement->statement_name, "return") != 0);
            }
            Statement * last = array_last(block->statements, Statement *);
            assert(strcmp(last->statement_name, "if") == 0 ||
                   strcmp(last->statement_name, "goto") == 0 ||
                   strcmp(last->statement_name, "return") == 0);
            
            if (strcmp(last->statement_name, "return") == 0)
            {
                if (func->return_type.variant == TYPE_NONE)
                    assert(((void)"Return type mismatch.", array_len(last->args, Operand) == 0));
                else
                {
                    assert(((void)"Return type mismatch.", array_len(last->args, Operand) == 1));
                    assert(((void)"Return type mismatch.", types_same(last->args[0].value->type, func->return_type)));
                }
            }
        }
    }
}

void print_ir_to(FILE * f, Program * program)
{
    if (f == 0)
        f = stdout;
    for (size_t fn = 0; fn < array_len(program->functions, Function *); fn++)
    {
        Function * func = program->functions[fn];
        fprintf(f, "func %s", func->name);
        if (func->return_type.variant != TYPE_NONE)
            fprintf(f, " returns %s", type_to_static_string(func->return_type));
        fprintf(f, "\n");
        for (size_t i = 0; i < array_len(func->args, Value *); i++)
        {
            Value * arg = func->args[i];
            if (arg->regalloced)
                fprintf(f, "    arg %s %s # reg %zd\n", arg->arg, type_to_static_string(arg->type), arg->regalloc);
            else
                fprintf(f, "    arg %s %s\n", arg->arg, type_to_static_string(arg->type));
        }
        for (size_t i = 0; i < array_len(func->stack_slots, Value *); i++)
        {
            StackSlot * slot = func->stack_slots[i]->slotinfo;
            fprintf(f, "    stack_slot %s %zu\n", slot->name, slot->size);
        }
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            if (b != 0)
                fprintf(f, "block %s\n", block->name);
            for (size_t i = 0; i < array_len(block->args, Value *); i++)
            {
                Value * arg = block->args[i];
                if (arg->regalloced)
                    fprintf(f, "    arg %s %s # reg %zd\n", arg->arg, type_to_static_string(arg->type), arg->regalloc);
                else
                    fprintf(f, "    arg %s %s\n", arg->arg, type_to_static_string(arg->type));
            }
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                if (statement->output)
                    fprintf(f, "    %s = %s", statement->output_name, statement->statement_name);
                else
                    fprintf(f, "    %s", statement->statement_name);
                
                size_t opcount = array_len(statement->args, Operand);
                
                for (size_t j = 0; j < opcount; j++)
                {
                    if (j == 1 && strcmp(statement->statement_name, "if") == 0)
                        fprintf(f, " goto");
                    
                    Operand op = statement->args[j];
                    if (op.variant == OP_KIND_INVALID)
                        fprintf(f, " <invalidoperand>");
                    else if (op.variant == OP_KIND_SEPARATOR)
                        fprintf(f, " else");
                    else if (op.variant == OP_KIND_TEXT)
                        fprintf(f, " %s", op.text);
                    else if (op.variant == OP_KIND_TYPE)
                        fprintf(f, " %s", type_to_static_string(op.rawtype));
                    else if (op.variant == OP_KIND_VALUE)
                    {
                        Value * value = op.value;
                        assert(value);
                        if (value->variant == VALUE_INVALID)
                            fprintf(f, " <invalidvalue>");
                        else if (value->variant == VALUE_ARG)
                            fprintf(f, " %s", value->arg);
                        else if (value->variant == VALUE_STACKADDR)
                            fprintf(f, " %s", value->slotinfo->name);
                        else if (value->variant == VALUE_SSA)
                            fprintf(f, " %s", value->ssa->output_name);
                        else if (value->variant == VALUE_CONST)
                        {
                            double nd;
                            float nf;
                            memcpy(&nd, &value->constant, 8);
                            memcpy(&nf, &value->constant, 4);
                            if (type_is_int(value->type))
                                fprintf(f, " %zu%s", value->constant, type_to_static_string(value->type));
                            else if (value->type.variant == TYPE_IPTR)
                                fprintf(f, " %zu%s", value->constant, type_to_static_string(value->type));
                            else if (value->type.variant == TYPE_F64)
                                fprintf(f, " %f%s", nd, type_to_static_string(value->type));
                            else if (value->type.variant == TYPE_F32)
                                fprintf(f, " %f%s", nf, type_to_static_string(value->type));
                            else
                                fprintf(f, " <aggregate>");
                        }
                    }
                    else if (op.variant == OP_KIND_RAWINTEGER)
                        fprintf(f, " %zd", op.rawint);
                    else
                        assert(0);
                }
                
                if (statement->output)
                    fprintf(f, " # edges_out len: %zu", array_len(statement->output->edges_out, Statement *));
                
                if (statement->output && statement->output->regalloced)
                    fprintf(f, " # reg: %zd", statement->output->regalloc);
                
                if (statement->num)
                    fprintf(f, " # num: %zd", statement->num);
                
                fprintf(f, "\n");
            }
        }
        fprintf(f, "endfunc\n");
    }
    fflush(f);
}

#endif // BBAE_COMPILER_COMMON
