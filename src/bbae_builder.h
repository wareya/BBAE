#ifndef BBAE_BUILDER
#define BBAE_BUILDER

#include "bbae_api.h"

/// Include this header file (bbae_builder.h) to construct BBAE code in memory, non-textually.

/// In addition to the functions defined here, you also want these from bbae_construction.h:
///
/// Type basic_type(enum BBAE_TYPE_VARIANT val)
/// Gets a Type object describing a given basic (primitive) type.
/// Basic types: TYPE_NONE, TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64, TYPE_IPTR, TYPE_F32, TYPE_F64
///
/// size_t type_size(Type type)
/// Return the size of a type.
///
/// Program * create_empty_program(void)
/// Creates a program to further operate on.
///
/// Function * create_function(Program * program, const char * name, Type return_type)
/// Creates a new function, including its initial entry block, and makes it the implicitly-active function.
///
/// Value * add_funcarg(Program * program, const char * name, Type type)
/// Add an argument to a function.
///
/// Value * add_stack_slot(Function * func, const char * name, uint64_t size)
/// Adds a stack slot to a function.
///
/// Block * create_block(Program * program, const char * name)
/// Creates a new block in the currently-active function and makes it the program's implicitly-active block.
/// (Some operations depend on what the current implicitly-active block is.)
/// If the given name is null, gives the block a dummy name.
///
/// Value * add_blockarg(Program * program, const char * name, Type type)
/// Adds an argument to a block. BBAE uses block arguments instead of phi nodes.
///
/// Statement * init_statement(const char * statement_name)
/// Creates and returns an unconfigured statement with the given name.
///
/// void statement_add_value_op(Statement * statement, Value * val)
/// Append a new value to a statement's operand list.
///
/// void statement_add_text_op(Statement * statement, const char * text)
/// Append new text (e.g. label, symbol name, NOT string value) to a statement's operand list.
///
/// void block_append_statement(Block * block, Statement * statement)
/// Inserts a configured statement at the end of a block.
/// Might create additional statements before the given statement and rewrite its operands to be legal.
///
/// Value * build_constant_f32(float f)
/// Value * build_constant_f64(double f)
/// Value * build_constant_i64(uint64_t n)
/// Value * build_constant_iptr(uint64_t n)
/// Value * build_constant_i32(uint32_t n)
/// Value * build_constant_i16(uint16_t n)
/// Value * build_constant_i8(uint8_t n)
/// Value * build_constant_zero(enum BBAE_TYPE_VARIANT variant)
/// Build constants of various primitive types.
///
/// You also want these from bbae_api.h:
///
/// void do_optimization(Program * program)
/// Perform all supported optimizations to all the functions in the given program.
///
/// byte_buffer * do_lowering(Program * program, SymbolEntry ** symbollist)
/// Lower the program to machine code and produce a symbol list (for function addresses, static inline data addresses, and global variable addresses). See `buffers.h` for the API for the byte_buffer class.
///
///
/// Typical usage first creates a program.
/// Then a function, then adds blocks to that function, adding statements to each block as needed, for each function.
/// Then runs optimizations.
/// Then runs lowering.


/// Initializes a statement object with the given operation name and gives it an output.
// (contains a giant list of operation names, so is defined at the bottom)
static inline Statement * init_statement_auto_output(const char * statement_name);

/// Null if the statement is an instruction instead of an operation.
static inline Value * statement_get_output(Statement * statement)
{
    return statement->output;
}
static inline Block * function_get_entry_block(Function * func)
{
    return func->entry_block;
}
static inline Statement * block_get_last_statement(Block * block)
{
    if (array_len(block->statements, Statement *) == 0)
        return 0;
    return block->statements[array_len(block->statements, Statement *) - 1];
}
static inline uint8_t block_is_terminated(Block * block)
{
    return statement_is_terminator(block_get_last_statement(block));
}

static inline Statement * build_statement_3val(Block * block, const char * statement_name, Value * a, Value * b, Value * c)
{
    Statement * ret = init_statement_auto_output(statement_name);
    statement_add_value_op(ret, a);
    statement_add_value_op(ret, b);
    statement_add_value_op(ret, c);
    block_append_statement(block, ret);
    return ret;
}

static inline Statement * build_statement_2val(Block * block, const char * statement_name, Value * a, Value * b)
{
    Statement * ret = init_statement_auto_output(statement_name);
    statement_add_value_op(ret, a);
    statement_add_value_op(ret, b);
    block_append_statement(block, ret);
    return ret;
}

static inline Statement * build_statement_typeval(Block * block, const char * statement_name, Type t, Value * a)
{
    Statement * ret = init_statement_auto_output(statement_name);
    statement_add_type_op(ret, t);
    statement_add_value_op(ret, a);
    block_append_statement(block, ret);
    return ret;
}

static inline Statement * build_statement_1val(Block * block, const char * statement_name, Value * a)
{
    Statement * ret = init_statement_auto_output(statement_name);
    statement_add_value_op(ret, a);
    block_append_statement(block, ret);
    return ret;
}

static inline Statement * build_statement_0val(Block * block, const char * statement_name)
{
    Statement * ret = init_statement_auto_output(statement_name);
    block_append_statement(block, ret);
    return ret;
}

static inline Statement * build_store(Block * block, Value * a, Value * b)
{
    Statement * ret = init_statement_auto_output("store");
    statement_add_value_op(ret, a);
    statement_add_value_op(ret, b);
    block_append_statement(block, ret);
    return ret;
}
static inline Statement * build_load(Block * block, Type a, Value * b)
{
    Statement * ret = init_statement_auto_output("load");
    statement_add_type_op(ret, a);
    statement_add_value_op(ret, b);
    block_append_statement(block, ret);
    return ret;
}

static inline Statement * build_return_void(Block * block)
{
    return build_statement_0val(block, "return");
}
static inline Statement * build_return_1val(Block * block, Value * a)
{
    return build_statement_1val(block, "return", a);
}

static inline Statement * build_add(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "add", a, b);
}
static inline Statement * build_sub(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "sub", a, b);
}
static inline Statement * build_mul(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "mul", a, b);
}
static inline Statement * build_imul(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "imul", a, b);
}

/// @brief Non-unsafe arithmetic functions avoid generating unspecified or undefined or trap values. See BBAE docs for more info.
static inline Statement * build_div(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "div" : "div_unsafe", a, b);
}
static inline Statement * build_idiv(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "idiv" : "idiv_unsafe", a, b);
}
static inline Statement * build_rem(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "rem" : "rem_unsafe", a, b);
}
static inline Statement * build_irem(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "irem" : "irem_unsafe", a, b);
}

static inline Statement * build_shl(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "shl" : "shl_unsafe", a, b);
}
static inline Statement * build_shr(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "shr" : "shr_unsafe", a, b);
}
static inline Statement * build_sar(Block * block, Value * a, Value * b, uint8_t is_unsafe)
{
    return build_statement_2val(block, !is_unsafe ? "sar" : "sar_unsafe", a, b);
}

static inline Statement * build_and(Block * block, Value * a)
{
    return build_statement_1val(block, "and", a);
}
static inline Statement * build_or(Block * block, Value * a)
{
    return build_statement_1val(block, "or", a);
}
static inline Statement * build_xor(Block * block, Value * a)
{
    return build_statement_1val(block, "xor", a);
}
static inline Statement * build_bitnot(Block * block, Value * a)
{
    return build_statement_1val(block, "bitnot", a);
}

static inline Statement * build_not(Block * block, Value * a)
{
    return build_statement_1val(block, "not", a);
}
static inline Statement * build_bool(Block * block, Value * a)
{
    return build_statement_1val(block, "bool", a);
}
static inline Statement * build_neg(Block * block, Value * a)
{
    return build_statement_1val(block, "neg", a);
}

static inline Statement * build_trim(Block * block, Value * a)
{
    return build_statement_1val(block, "trim", a);
}
static inline Statement * build_qext(Block * block, Value * a)
{
    return build_statement_1val(block, "qext", a);
}
static inline Statement * build_zext(Block * block, Value * a)
{
    return build_statement_1val(block, "zext", a);
}
static inline Statement * build_sext(Block * block, Value * a)
{
    return build_statement_1val(block, "sext", a);
}


static inline Statement * build_fadd(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "fadd", a, b);
}
static inline Statement * build_fsub(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "fsub", a, b);
}
static inline Statement * build_fmul(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "fmul", a, b);
}
static inline Statement * build_fdiv(Block * block, Value * a, Value * b)
{
    return build_statement_2val(block, "fdiv", a, b);
}
/*
static inline Statement * build_fsqrt(Block * block, Value * a)
{
    return build_statement_1val(block, "fsqrt", a);
}
*/

static inline Statement * build_fneg(Block * block, Value * a)
{
    return build_statement_1val(block, "fneg", a);
}
static inline Statement * build_fbool(Block * block, Value * a)
{
    return build_statement_1val(block, "fbool", a);
}
static inline Statement * build_fisnan(Block * block, Value * a)
{
    return build_statement_1val(block, "fisnan", a);
}

static inline Statement * build_f32_to_f64(Block * block, Value * a)
{
    return build_statement_1val(block, "f32_to_f64", a);
}
static inline Statement * build_f64_to_f32(Block * block, Value * a)
{
    return build_statement_1val(block, "f64_to_f32", a);
}

static inline Statement * build_uint_to_float(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "uint_to_float", t, a);
}
static inline Statement * build_float_to_uint(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "float_to_uint", t, a);
}
static inline Statement * build_float_to_uint_unsafe(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "float_to_uint_unsafe", t, a);
}

static inline Statement * build_sint_to_float(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "sint_to_float", t, a);
}
static inline Statement * build_float_to_sint(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "float_to_sint", t, a);
}
static inline Statement * build_float_to_sint_unsafe(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "float_to_sint_unsafe", t, a);
}

static inline Statement * build_bitcast(Block * block, Type t, Value * a)
{
    return build_statement_typeval(block, "bitcast", t, a);
}

enum CmpComparison {
    CMP_EQ,
    CMP_NE,
    CMP_GE,
    CMP_G,
    CMP_LE,
    CMP_L,
};

static inline Statement * build_ucmp(Block * block, Value * a, Value * b, enum CmpComparison cmp)
{
    assert(a && b);
    assert(types_same(a->type, b->type));
    const char * s =
        cmp == CMP_EQ ? "cmp_eq" :
        cmp == CMP_NE ? "cmp_ne" :
        cmp == CMP_GE ? "cmp_ge" :
        cmp == CMP_G  ? "cmp_g"  :
        cmp == CMP_LE ? "cmp_le" :
        cmp == CMP_L  ? "cmp_l"  :
        (assert(((void)"invalid kind-of-comparison input to comparison builder", 0)), "");
    return build_statement_2val(block, s, a, b);
}
static inline Statement * build_icmp(Block * block, Value * a, Value * b, enum CmpComparison cmp)
{
    assert(a && b);
    assert(types_same(a->type, b->type));
    const char * s =
        cmp == CMP_EQ ? "cmp_eq" :
        cmp == CMP_NE ? "cmp_ne" :
        cmp == CMP_GE ? "icmp_ge" :
        cmp == CMP_G  ? "icmp_g"  :
        cmp == CMP_LE ? "icmp_le" :
        cmp == CMP_L  ? "icmp_l"  :
        (assert(((void)"invalid kind-of-comparison input to comparison builder", 0)), "");
    return build_statement_2val(block, s, a, b);
}
static inline Statement * build_fcmp(Block * block, Value * a, Value * b, enum CmpComparison cmp)
{
    assert(a && b);
    assert(types_same(a->type, b->type));
    const char * s =
        cmp == CMP_EQ ? "fcmp_eq" :
        cmp == CMP_NE ? "fcmp_ne" :
        cmp == CMP_GE ? "fcmp_ge" :
        cmp == CMP_G  ? "fcmp_g"  :
        cmp == CMP_LE ? "fcmp_le" :
        cmp == CMP_L  ? "fcmp_l"  :
        (assert(((void)"invalid kind-of-comparison input to comparison builder", 0)), "");
    return build_statement_2val(block, s, a, b);
}

static inline Statement * init_statement_auto_output(const char * statement_name)
{
    if (strcmp(statement_name, "add") == 0 ||
        strcmp(statement_name, "sub") == 0 ||
        strcmp(statement_name, "mul") == 0 ||
        strcmp(statement_name, "imul") == 0 ||
        strcmp(statement_name, "div") == 0 ||
        strcmp(statement_name, "idiv") == 0 ||
        strcmp(statement_name, "rem") == 0 ||
        strcmp(statement_name, "irem") == 0 ||
        strcmp(statement_name, "shl") == 0 ||
        strcmp(statement_name, "shr") == 0 ||
        strcmp(statement_name, "sar") == 0 ||
        
        strcmp(statement_name, "fadd") == 0 ||
        strcmp(statement_name, "fsub") == 0 ||
        strcmp(statement_name, "fmul") == 0 ||
        strcmp(statement_name, "fdiv") == 0 ||
        strcmp(statement_name, "fxor") == 0 ||
        
        strcmp(statement_name, "mov") == 0 ||
        
        strcmp(statement_name, "load") == 0 ||
        strcmp(statement_name, "trim") == 0 ||
        strcmp(statement_name, "qext") == 0 ||
        strcmp(statement_name, "sext") == 0 ||
        strcmp(statement_name, "zext") == 0 ||
        
        strcmp(statement_name, "neg") == 0 ||
        strcmp(statement_name, "fneg") == 0 ||
        
        strcmp(statement_name, "float_to_uint") == 0 ||
        strcmp(statement_name, "float_to_uint_unsafe") == 0 ||
        strcmp(statement_name, "uint_to_float") == 0 ||
        strcmp(statement_name, "float_to_sint") == 0 ||
        strcmp(statement_name, "float_to_sint_unsafe") == 0 ||
        strcmp(statement_name, "sint_to_float") == 0 ||
        
        strcmp(statement_name, "bitcast") == 0 ||
        
        strcmp(statement_name, "extract") == 0 ||
        
        strcmp(statement_name, "cmp_g") == 0 ||
        strcmp(statement_name, "cmp_ge") == 0 ||
        strcmp(statement_name, "cmp_l") == 0 ||
        strcmp(statement_name, "cmp_le") == 0 ||
        strcmp(statement_name, "cmp_eq") == 0 ||
        strcmp(statement_name, "cmp_ne") == 0 ||
        strcmp(statement_name, "icmp_g") == 0 ||
        strcmp(statement_name, "icmp_ge") == 0 ||
        strcmp(statement_name, "icmp_l") == 0 ||
        strcmp(statement_name, "icmp_le") == 0 ||
        strcmp(statement_name, "fcmp_g") == 0 ||
        strcmp(statement_name, "fcmp_ge") == 0 ||
        strcmp(statement_name, "fcmp_l") == 0 ||
        strcmp(statement_name, "fcmp_le") == 0 ||
    
        strcmp(statement_name, "symbol_lookup_unsized") == 0 ||
        strcmp(statement_name, "symbol_lookup") == 0 ||
        
        strcmp(statement_name, "call_eval") == 0 ||
        strcmp(statement_name, "call") == 0)
    {
        Statement * statement = init_statement(statement_name);
        statement->output_name = make_temp_name();
        return statement;
    }
    else
        return init_statement(statement_name);
}
        
#endif // BBAE_BUILDER
