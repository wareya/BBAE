#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"
#include "emitter.h"
#include "abi.h"

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

uint64_t size_guess_align(uint64_t s)
{
    uint64_t ret = 1;
    while (ret < 64 && ret < s)
        ret <<= 1;
    return ret;
}

enum BBAE_TYPE_VARIANT {
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

Type basic_type(int x)
{
    Type ret;
    memset(&ret, 0, sizeof(Type));
    ret.variant = x;
    return ret;
}

uint8_t type_is_basic(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_F64;
}
uint8_t type_is_int(Type type)
{
    return type.variant >= TYPE_I8 && type.variant <= TYPE_I64;
}
uint8_t type_is_float(Type type)
{
    return type.variant >= TYPE_F32 && type.variant <= TYPE_F64;
}

size_t type_size(Type type)
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
    VALUE_NONE,
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
    // array
    Value ** edges_in;
} Statement;

Statement * new_statement(void)
{
    Statement * statement = (Statement *)zero_alloc(sizeof(Statement));
    statement->args = (Operand *)zero_alloc(0);
    statement->args_a = (Operand *)zero_alloc(0);
    statement->args_b = (Operand *)zero_alloc(0);
    statement->edges_in = (Value **)zero_alloc(0);
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

Block * new_block(void)
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

Function new_func(void)
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

enum BBAE_PARSER_STATE {
    PARSER_STATE_ROOT,
    PARSER_STATE_FUNCARGS,
    PARSER_STATE_FUNCSLOTS,
    PARSER_STATE_BLOCKARGS,
    PARSER_STATE_BLOCK,
};

Program program;
Function current_func;
Block * current_block = 0;

Value * make_value(Type type)
{
    Value * ret = (Value *)zero_alloc(sizeof(Value));
    ret->edges_out = (Statement **)zero_alloc(0);
    ret->type = type;
    return ret;
}

Value * make_const_value(enum BBAE_TYPE_VARIANT variant, uint64_t data)
{
    Value * ret = make_value(basic_type(variant));
    ret->variant = VALUE_CONST;
    ret->constant = data;
    return ret;
}

Value * make_stackslot_value(StackSlot * slot)
{
    Value * ret = make_value(basic_type(TYPE_I64));
    ret->variant = VALUE_STACKADDR;
    ret->slotinfo = slot;
    return ret;
}

Value * parse_value(char * token)
{
    assert(token);
    // tokens
    assert(strcmp(token, "=") != 0);
    assert(strcmp(token, "{") != 0);
    assert(strcmp(token, "}") != 0);
    assert(strcmp(token, "<=") != 0);
    
    if ((token[0] >= '0' && token[0] <= '9')
        || token[0] == '-'|| token[0] == '.')
    {
        Value * ret = make_value(basic_type(TYPE_NONE));
        
        if (str_ends_with(token, "f32"))
        {
            char * end = 0;
            float f = strtof(token, &end);
            assert(("invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            ret->variant = VALUE_CONST;
            ret->type = basic_type(TYPE_F32);
            memcpy(&ret->constant, &f, 4);
        }
        else if (str_ends_with(token, "f64"))
        {
            char * end = 0;
            double f = strtod(token, &end);
            assert(("invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            ret->variant = VALUE_CONST;
            ret->type = basic_type(TYPE_F64);
            memcpy(&ret->constant, &f, 8);
        }
        else if (str_ends_with(token, "i8") || str_ends_with(token, "i16")
                 || str_ends_with(token, "i32") || str_ends_with(token, "i64"))
        {
            //uint8_t suffix_len = str_ends_with(token, "i8") ? 2 : 3;
            uint64_t n = parse_int_nonbare(token);
            ret->variant = VALUE_CONST;
            if (str_ends_with(token, "i8"))
                ret->type = basic_type(TYPE_I8);
            else if (str_ends_with(token, "i16"))
                ret->type = basic_type(TYPE_I16);
            else if (str_ends_with(token, "i32"))
                ret->type = basic_type(TYPE_I32);
            else if (str_ends_with(token, "i64"))
                ret->type = basic_type(TYPE_I64);
            else
                assert(0);
            ret->constant = n;
        }
        else
        {
            assert(("unknown type of literal value", 0));
        }
        
        return ret;
    }
    else
    {
        Value ** args = 0;
        if (current_block == current_func.entry_block)
            args = current_func.args;
        else
            args = current_block->args;
        
        for (size_t i = 0; i < array_len(args, Value *); i++)
        {
            Value * val = args[i];
            assert(val->variant == VALUE_ARG);
            if (strcmp(val->arg, token) == 0)
                return val;
        }
        
        for (size_t i = 0; i < array_len(current_func.stack_slots, Value *); i++)
        {
            Value * value = current_func.stack_slots[i];
            assert(value->variant == VALUE_STACKADDR);
            StackSlot * slot = value->slotinfo;
            if (strcmp(slot->name, token) == 0)
                return value;
        }
        
        for (size_t i = 0; i < array_len(current_block->statements, Statement *); i++)
        {
            Statement * statement = current_block->statements[i];
            if (statement->output_name && strcmp(statement->output_name, token) == 0)
            {
                if (!statement->output)
                {
                    printf("culprit: %s\n", token);
                    assert(("tried to use output of operation before it was run", 0));
                }
                return statement->output;
            }
        }
        
        printf("culprit: %s\n", token);
        assert(("tried to use unknown variable", 0));
    }
}


uint8_t op_is_v(char * opname)
{
    const char * ops[] = {
        "bnot", "not", "bool", "neg", "f32_to_f64", "f64_to_f32", "freeze", "ptralias_bless", "stack_addr"
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(char *); i++)
    {
        if (strcmp(opname, ops[i]) == 0)
            return 1;
    }
    return 0;
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

Operand new_op_val(Value * val)
{
    Operand op;
    op.variant = OP_KIND_VALUE;
    op.value = val;
    return op;
}
Operand parse_op_val(char ** cursor)
{
    return new_op_val(parse_value(find_next_token(cursor)));
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
        char * token_1 = strcpy_z(find_next_token(cursor));
        ret->statement_name = token_1;
        
        if (op_is_v_v(ret->statement_name))
        {
            char * op1_text = strcpy_z(find_next_token(cursor));
            char * op2_text = strcpy_z(find_next_token(cursor));
            
            Operand op1 = parse_op_val(&op1_text);
            Operand op2 = parse_op_val(&op2_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, op2);
            
            return ret;
        }
        else if (op_is_v(ret->statement_name))
        {
            char * op1_text = strcpy_z(find_next_token(cursor));
            Operand op1 = parse_op_val(&op1_text);
            array_push(ret->args, Operand, op1);
            return ret;
        }
        else
        {
            assert(("glfy39", 0));
        }
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
                op.variant = OP_KIND_VALUE;
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

uint64_t temp_ctr = 0;

char * make_temp_var_name(void)
{
    size_t len = snprintf(0, 0, "__bbae_temp_%llu", temp_ctr);
    char * str = zero_alloc(len + 1);
    snprintf(str, len, "__bbae_temp_%llu", temp_ctr);
    return str;
}

void add_statement_output(Statement * statement)
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

void fix_last_statement_stack_slot_addr_usage(void)
{
    size_t end = array_len(current_block->statements, Statement *);
    if (end == 0)
        return;
    Statement * statement = current_block->statements[end - 1];
    // pre-process any stack slot address arguments into explicit address access instructions
    if (strcmp(statement->statement_name, "load") != 0 &&
        strcmp(statement->statement_name, "store") != 0 &&
        strcmp(statement->statement_name, "stack_addr") != 0)
    {
        for (size_t i = 0; i < array_len(statement->args, Operand); i++)
        {
            Value * value = statement->args[i].value;
            if (!value)
                continue;
            if (value->variant == VALUE_STACKADDR)
            {
                Statement * s = new_statement();
                char * str = make_temp_var_name();
                s->output_name = str;
                s->statement_name = strcpy_z("stack_addr");
                
                // move around args
                array_push(s->args, Operand, statement->args[i]);
                
                add_statement_output(s);
                Operand op;
                op.variant = OP_KIND_VALUE;
                op.value = s->output;
                statement->args[i] = op;
                
                // swap IDs
                /*
                uint64_t temp_id = s->id;
                s->id = statement->id;
                statement->id = temp_id;
                */
                
                // swap locations
                current_block->statements[end - 1] = s;
                array_push(current_block->statements, Statement *, statement);
                
                // update "end"
                end = array_len(current_block->statements, Statement *);
            }
        }
    }
}

Value * add_stack_slot(Function * func, char * name, uint64_t size)
{            
    StackSlot _slot = {name, size, 0, 0};
    StackSlot * slot = (StackSlot *)zero_alloc(sizeof(StackSlot));
    *slot = _slot;
    Value * val = make_stackslot_value(slot);
    array_push(func->stack_slots, Value *, val);
    return val;
}

int parse_file(char * cursor)
{
    enum BBAE_PARSER_STATE state = PARSER_STATE_ROOT;
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
                
                assert(("todo", 0));
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
                
                Value * value = make_value(type);
                value->variant = VALUE_ARG;
                value->arg = name;
                array_push(current_func.args, Value *, value);
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
                
                add_stack_slot(&current_func, name, size);
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
                add_statement_output(statement);
                array_push(current_block->statements, Statement *, statement);
                fix_last_statement_stack_slot_addr_usage();
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

void connect_statement_to_operand(Statement * statement, Operand op)
{
    if (op.variant == OP_KIND_VALUE)
    {
        Value * val = op.value;
        
        if (val->variant == VALUE_ARG || val->variant == VALUE_SSA || val->variant == VALUE_STACKADDR)
        {
            /*
            if (val->slotinfo)
                printf("connecting %s (%p) to statement %llu\n", val->slotinfo->name, (void *)val, statement->id);
            else if (val->ssa)
                printf("connecting %s to statement %llu\n", val->ssa->output_name, statement->id);
            */
            array_push(statement->edges_in, Value *, val);
            array_push(val->edges_out, Statement *, statement);
            
            //val->last_edge = statement;
            //printf("%llu ???\n", val->last_edge->id);
            //if (!val->last_edge || val->last_edge->id < statement->id)
            //    val->last_edge = statement;
        }
    }
}

// split blocks if they have any conditional jumps
void split_blocks(void)
{
    // TODO: do a pre-pass collecting the final use of each variable/argument.
    // This will be redundant work vs the graph connection pass, but has to happen separately
    // because the graph connection pass would point to things from the previous block
    // with the later block needing to be patched intrusively. This is simpler.
    
    for (size_t f = 0; f < array_len(program.functions, Function); f++)
    {
        Function * func = &program.functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                // TODO: log operation outputs to use in later args
                if (strcmp(statement->statement_name, "if") == 0)
                {
                    assert(("TODO: split block, convey arguments", 0));
                }
            }
        }
    }
}

void connect_graphs(void)
{
    split_blocks();
    
    for (size_t f = 0; f < array_len(program.functions, Function); f++)
    {
        Function * func = &program.functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                for (size_t n = 0; n < array_len(statement->args, Operand); n++)
                    connect_statement_to_operand(statement, statement->args[n]);
                for (size_t n = 0; n < array_len(statement->args_a, Operand); n++)
                    connect_statement_to_operand(statement, statement->args[n]);
                for (size_t n = 0; n < array_len(statement->args_b, Operand); n++)
                    connect_statement_to_operand(statement, statement->args[n]);
                
                if (array_len(statement->args_a, Operand) > 0)
                {
                    assert(("TODO: block lookup and connect blocks together", 0));
                }
            }
        }
    }
}

void allocate_stack_slots(void)
{
    for (size_t f = 0; f < array_len(program.functions, Function); f++)
    {
        Function * func = &program.functions[f];
        uint64_t offset = 0;
        for (size_t s = 0; s < array_len(func->stack_slots, Value *); s++)
        {
            assert(func->stack_slots[s]->variant == VALUE_STACKADDR);
            StackSlot * slot = func->stack_slots[s]->slotinfo;
            uint64_t align = size_guess_align(slot->size);
            offset += slot->size;
            while (offset % align)
                offset += 1;
            slot->offset = offset;
        }
    }
}

int64_t first_empty(Value ** array, size_t len)
{
    int64_t found = -1;
    for (size_t n = 0; n < len; n++)
    {
        if (!array[n])
        {
            found = n;
            break;
        }
    }
    return found;
}

void do_regalloc_block(Function * func, Block * block)
{
    Value * reg_int_alloced[16];
    Value * reg_float_alloced[16];
    
    memset(reg_int_alloced, 0, 16 * sizeof(Value *));
    reg_int_alloced[REG_RSP] = (Value *)-1;
    reg_int_alloced[REG_RBP] = (Value *)-1;
    memset(reg_float_alloced, 0, 16 * sizeof(Value *));
    
    if (block == func->entry_block)
    {
        // special case for arguments
        abi_reset_state();
        for (size_t i = 0; i < array_len(func->args, Value *); i++)
        {
            Value * value = func->args[i];
            if (array_len(value->edges_out, Statement *) == 0)
                continue;
            if (type_is_basic(value->type))
            {
                int64_t where = abi_get_next_arg_basic(type_is_float(value->type));
                assert(("arg reg spill not yet supported", where >= 0));
                if (where >= _ABI_XMM0)
                    reg_float_alloced[where - _ABI_XMM0] = value;
                else 
                    reg_int_alloced[where] = value;
                
                printf("allocated register %lld to arg %s\n", where, value->arg);
                
                value->regalloc = where;
                value->regalloced = 1;
            }
            else
                assert(("aggregate args not yet supported", 0));
        }
    }
    else
    {
        // TODO: allocate based on output allocation of entry blocks
        assert(("TODO later block regalloc", 0));
    }
    
    // give statements numbers for regalloc reasons
    for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
        block->statements[i]->num = i;
    
    for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
    {
        Statement * statement = block->statements[i];
        
        // free no-longer-used registers, except for RSP/RBP
        // commented out to implement spilling
        /*
        for (size_t n = 0; n < 16; n++)
        {
            if (reg_int_alloced[n] && reg_int_alloced[n] != (Value *)-1 && n != REG_RSP && n != REG_RBP)
            {
                Value * value = reg_int_alloced[n];
                assert(value->alloced_use_count <= array_len(value->edges_out, Value *));
                if (value->alloced_use_count == array_len(value->edges_out, Value *))
                {
                    printf("freeing int register %llu... %llu vs %llu\n", n, value->alloced_use_count, array_len(value->edges_out, Value *));
                    reg_int_alloced[n] = 0;
                }
            }
            if (reg_float_alloced[n] && reg_float_alloced[n] != (Value *)-1)
            {
                Value * value = reg_float_alloced[n];
                assert(value->alloced_use_count <= array_len(value->edges_out, Value *));
                if (value->alloced_use_count == array_len(value->edges_out, Value *))
                {
                    printf("freeing float register %llu...\n", n);
                    reg_float_alloced[n] = 0;
                }
            }
        }
        */
        
        for (size_t j = 0; j < array_len(statement->args, Operand); j++)
        {
            Value * arg = statement->args[j].value;
            if (arg)
            {
                arg->alloced_use_count += 1;
                assert(arg->alloced_use_count <= array_len(arg->edges_out, Value *));
            }
        }
        
        if (!statement->output)
            continue;
        
        assert(("TODO", type_is_int(statement->output->type)));
        
        // reuse an operand register if possible
        for (size_t j = 0; j < array_len(statement->args, Operand); j++)
        {
            Value * arg = statement->args[j].value;
            if (arg && arg->regalloced
                && arg->alloced_use_count == array_len(arg->edges_out, Value *))
            {
                statement->output->regalloc = arg->regalloc;
                statement->output->regalloced = 1;
                
                int64_t where = arg->regalloc;
                
                if (where >= _ABI_XMM0)
                    reg_float_alloced[where - _ABI_XMM0] = statement->output;
                else 
                    reg_int_alloced[where] = statement->output;
                
                goto early_cont;
            }
        }
        if (0)
        {
            early_cont:
            continue;
        }
        
        int64_t where = first_empty(reg_int_alloced, 16);
        if (where < 0)
        {
            // pick the register whose next use is the furthest into the future
            int64_t to_spill = -1;
            uint64_t to_spill_num = 0;
            for (size_t n = 0; n < 16; n++)
            {
                if (!(reg_int_alloced[n] && reg_int_alloced[n] != (Value *)-1 && n != REG_RSP && n != REG_RBP))
                    continue;
                
                Value * candidate = reg_int_alloced[n];
                
                // check if spill candidate is an input. skip if it is.
                for (size_t j = 0; j < array_len(statement->args, Operand); j++)
                {
                    if (statement->args[j].value == candidate)
                        goto force_skip;
                }
                if (0)
                {
                    force_skip:
                    continue;
                }
                
                uint64_t candidate_last_num = 0;
                for (size_t i = 0; i < array_len(candidate->edges_out, Value *); i++)
                {
                    uint64_t temp = candidate->edges_out[i]->num;
                    candidate_last_num = temp > candidate_last_num ? temp : candidate_last_num;
                }
                
                if (to_spill == -1 || candidate_last_num > to_spill_num)
                {
                    to_spill = n;
                    to_spill_num = candidate_last_num;
                    continue;
                }
            }
            assert(to_spill >= 0);
            
            printf("want to spill reg %llu\n", to_spill);
            Value * spillee = reg_int_alloced[to_spill];
            
            assert(spillee->regalloced);
            assert(spillee->regalloc >= 0);
            assert(!spillee->spilled);
            
            // spill statements don't need to be numbered because they're never an outward edge of an SSA value
            
            Value * spill_slot = add_stack_slot(func, make_temp_var_name(), type_size(statement->output->type));
            
            spillee->spilled = spill_slot->slotinfo;
            
            Statement * inst = new_statement();
            inst->statement_name = strcpy_z("store");
            array_push(inst->args, Operand, new_op_val(spill_slot));
            array_push(inst->args, Operand, new_op_val(spillee));
            
            array_insert(block->statements, Statement *, i, inst);
            i += 1;
            
            where = spillee->regalloc;
            
            // TODO: figure out unspilling
        }
        
        if (where >= _ABI_XMM0)
            reg_float_alloced[where - _ABI_XMM0] = statement->output;
        else 
            reg_int_alloced[where] = statement->output;
        
        //printf("allocating %lld to statement %llu (assigns to %s)\n", where, statement->id, statement->output_name);
        
        statement->output->regalloc = where;
        statement->output->regalloced = 1;
    }
}

void do_regalloc(void)
{
    for (size_t f = 0; f < array_len(program.functions, Function); f++)
    {
        Function * func = &program.functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            do_regalloc_block(func, block);
        }
    }
}

byte_buffer * code;

EncOperand get_basic_encoperand(Value * value)
{
    assert(value->variant == VALUE_CONST || value->variant == VALUE_STACKADDR || value->regalloced);
    if (value->variant == VALUE_CONST)
        return zy_imm(value->constant);
    else if (value->variant == VALUE_SSA || value->variant == VALUE_ARG)
        return zy_reg(value->regalloc, type_size(value->type));
    else if (value->variant == VALUE_STACKADDR)
        return zy_mem(REG_RBP, -value->slotinfo->offset, type_size(value->type));
    else
        assert(("TODO", 0));
}

void compile_file(void)
{
    code = (byte_buffer *)malloc(sizeof(byte_buffer));
    memset(code, 0, sizeof(byte_buffer));
    
    for (size_t f = 0; f < array_len(program.functions, Function); f++)
    {
        Function * func = &program.functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                
                if (strcmp(statement->statement_name, "return") == 0)
                {
                    Operand op = statement->args[0];
                    assert(op.variant == OP_KIND_VALUE);
                    
                    EncOperand op1 = zy_reg(REG_RAX, 8);
                    EncOperand op2 = get_basic_encoperand(op.value);
                    if (!encops_equal(op1, op2))
                        zy_emit_2(code, INST_MOV, op1, op2);
                    zy_emit_0(code, INST_RET);
                }
                else if (strcmp(statement->statement_name, "add") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    if (encops_equal(op0, op2))
                    {
                        EncOperand temp = op1;
                        op1 = op2;
                        op2 = temp;
                    }
                    if (!encops_equal(op0, op1))
                        zy_emit_2(code, INST_MOV, op0, op1);
                    zy_emit_2(code, INST_ADD, op0, op2);
                }
                else if (strcmp(statement->statement_name, "stack_addr") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    
                    zy_emit_2(code, INST_LEA, op0, op1);
                }
                else if (strcmp(statement->statement_name, "store") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    
                    zy_emit_2(code, INST_MOV, op1, op2);
                }
                else
                {
                    printf("culprit: %s\n", statement->statement_name);
                    assert(("unhandled operation!", 0));
                }
            }
        }
    }
}

int main(int argc, char ** argv)
{
    // array insert/erase test code
    /*
    uint8_t * test_array = (uint8_t *)zero_alloc(0);
    
    array_push(test_array, uint8_t, 0);
    array_push(test_array, uint8_t, 1);
    array_push(test_array, uint8_t, 2);
    array_push(test_array, uint8_t, 3);
    array_push(test_array, uint8_t, 4);
    array_push(test_array, uint8_t, 5);
    
    array_insert(test_array, uint8_t, 2, 121);
    
    for (size_t i = 0; i < array_len(test_array, uint8_t); i++)
        printf("%d...%d\n", i, test_array[i]);
    
    array_erase(test_array, uint8_t, 5);
    array_erase(test_array, uint8_t, 0);
    
    for (size_t i = 0; i < array_len(test_array, uint8_t); i++)
        printf("%d...%d\n", i, test_array[i]);
    */
   
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
    connect_graphs();
    do_regalloc();
    allocate_stack_slots();
    
    compile_file();
    
    for (size_t i = 0; i < code->len; i++)
        printf("%02X ", code->data[i]);
    puts("");
    
    puts("wah");
    
    return 0;
}
