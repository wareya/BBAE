#ifndef BBAE_BUILDER
#define BBAE_BUILDER

/// Include this header file to construct BBAE code in memory, non-textually.

#include "bbae_api.h"

/// In addition to the functions defined here, you also want these from bbae_construction:
///
/// Value * add_stack_slot(Function * func, const char * name, uint64_t size)
/// Adds a stack slot to a function.
///
/// Function * create_function(Program * program, const char * name, Type return_type)
/// Creates a new function, including its initial entry block.
///
/// Value * add_funcarg(Program * program, const char * name, Type type)
/// Add an argument to a function.
///
/// Block * create_block(Program * program, const char * name)
/// Creates a new block.
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

Value * statement_get_output(Statement * statement)
{
    return statement->output;
}

Statement * build_statement_2op(const char * statement_name, Value * a, Value * b)
{
    Statement * ret = init_statement(statement_name);
    statement_add_value_op(ret, a);
    statement_add_value_op(ret, b);
    return ret;
}

Statement * build_add(Value * a, Value * b)
{
    return build_statement_2op("add", a, b);
}

#endif // BBAE_BUILDER
