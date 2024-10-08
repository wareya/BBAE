#ifndef BBAE_CONSTRUCTION
#define BBAE_CONSTRUCTION

#include "compiler_common.h"

enum BBAE_PARSER_STATE {
    PARSER_STATE_ROOT,
    PARSER_STATE_FUNCARGS,
    PARSER_STATE_FUNCSLOTS,
    PARSER_STATE_BLOCKARGS,
    PARSER_STATE_BLOCK,
};

static inline uint8_t check_for_redefinition(Function * func, Block * block, const char * name)
{
    assert(func);
    
    Value ** args = block ? block->args : func->args;
    
    for (size_t i = 0; i < array_len(args, Value *); i++)
    {
        Value * val = args[i];
        assert(val->variant == VALUE_ARG);
        if (strcmp(val->arg, name) == 0)
            return 1;
    }
    for (size_t i = 0; i < array_len(func->stack_slots, Value *); i++)
    {
        Value * value = func->stack_slots[i];
        assert(value->variant == VALUE_STACKADDR);
        StackSlot * slot = value->slotinfo;
        if (strcmp(slot->name, name) == 0)
            return 1;
    }
    for (size_t i = 0; block && block->statements && i < array_len(block->statements, Statement *); i++)
    {
        Statement * statement = block->statements[i];
        if (statement->output_name && strcmp(statement->output_name, name) == 0)
            return 1;
    }
    return 0;
}

static inline void assert_no_redefinition(Function * func, Block * block, const char * name)
{
    if (check_for_redefinition(func, block, name))
    {
        printf("culprit: %s\n", name);
        assert(((void)"variable redefined!", 0));
    }
}

static inline Value * build_constant_f32(float f)
{
    Value * ret = make_value(basic_type(TYPE_F32));
    ret->variant = VALUE_CONST;
    memcpy(&ret->constant, &f, 4);
    return ret;
}
static inline Value * build_constant_f64(double f)
{
    Value * ret = make_value(basic_type(TYPE_F64));
    ret->variant = VALUE_CONST;
    memcpy(&ret->constant, &f, 8);
    return ret;
}
static inline Value * build_constant_i64(uint64_t n)
{
    Value * ret = make_value(basic_type(TYPE_I64));
    ret->variant = VALUE_CONST;
    ret->constant = n;
    return ret;
}
static inline Value * build_constant_iptr(uint64_t n)
{
    Value * ret = make_value(basic_type(TYPE_IPTR));
    ret->variant = VALUE_CONST;
    ret->constant = n;
    return ret;
}
static inline Value * build_constant_i32(uint32_t n)
{
    Value * ret = make_value(basic_type(TYPE_I32));
    ret->variant = VALUE_CONST;
    ret->constant = n;
    return ret;
}
static inline Value * build_constant_i16(uint16_t n)
{
    Value * ret = make_value(basic_type(TYPE_I16));
    ret->variant = VALUE_CONST;
    ret->constant = n;
    return ret;
}
static inline Value * build_constant_i8(uint8_t n)
{
    Value * ret = make_value(basic_type(TYPE_I8));
    ret->variant = VALUE_CONST;
    ret->constant = n;
    return ret;
}

static inline Value * build_constant_zero(enum BBAE_TYPE_VARIANT variant)
{
    if (variant == TYPE_I8)
        return build_constant_i8(0);
    else if (variant == TYPE_I16)
        return build_constant_i16(0);
    else if (variant == TYPE_I32)
        return build_constant_i32(0);
    else if (variant == TYPE_I64)
        return build_constant_i64(0);
    else if (variant == TYPE_IPTR)
        return build_constant_iptr(0);
    else if (variant == TYPE_F32)
        return build_constant_f32(0);
    else if (variant == TYPE_F64)
        return build_constant_f64(0);
    else
        assert(((void)"error: unsupported type for zero constant", 0));
}
/*
static inline Value * build_constant_poison(Type type)
{
    
}
*/

static inline Value * parse_value(Program * program, char * token)
{
    Function * func = program->current_func;
    Block * block = program->current_block;
    assert(func);
    assert(block);
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
            assert(((void)"invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            return build_constant_f32(f);
        }
        else if (str_ends_with(token, "f64"))
        {
            char * end = 0;
            double f = strtod(token, &end);
            assert(((void)"invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            return build_constant_f64(f);
        }
        else if (str_ends_with(token, "i8") || str_ends_with(token, "i16")
                 || str_ends_with(token, "i32") || str_ends_with(token, "i64")
                 || str_ends_with(token, "iptr"))
        {
            uint64_t n = parse_int_nonbare(token);
            //printf("parsed int... %zd\n", n);
            if (str_ends_with(token, "i8"))
                return build_constant_i8(n);
            else if (str_ends_with(token, "i16"))
                return build_constant_i16(n);
            else if (str_ends_with(token, "i32"))
                return build_constant_i32(n);
            else if (str_ends_with(token, "i64"))
                return build_constant_i64(n);
            else if (str_ends_with(token, "iptr"))
                return build_constant_iptr(n);
            else
                assert(0);
        }
        else
        {
            assert(((void)"unknown type of literal value", 0));
        }
        assert(((void)"unreachable", 0));
        return 0; // silence broken MSVC warning
    }
    else
    {
        Value ** args = 0;
        if (block == func->entry_block)
            args = func->args;
        else
            args = block->args;
        
        for (size_t i = 0; i < array_len(args, Value *); i++)
        {
            Value * val = args[i];
            assert(val->variant == VALUE_ARG);
            if (strcmp(val->arg, token) == 0)
                return val;
        }
        
        for (size_t i = 0; i < array_len(func->stack_slots, Value *); i++)
        {
            Value * value = func->stack_slots[i];
            assert(value->variant == VALUE_STACKADDR);
            StackSlot * slot = value->slotinfo;
            if (strcmp(slot->name, token) == 0)
                return value;
        }
        
        for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
        {
            Statement * statement = block->statements[i];
            if (statement->output_name && strcmp(statement->output_name, token) == 0)
            {
                if (!statement->output)
                {
                    printf("culprit: %s\n", token);
                    assert(((void)"tried to use output of operation before it was run", 0));
                }
                return statement->output;
            }
        }
        
        printf("culprit: %s\n", token);
        assert(((void)"tried to use unknown variable", 0));
        return 0; // silence broken MSVC warning
    }
}

static inline Operand parse_op_type(const char ** cursor)
{
    return new_op_type(parse_type(cursor));
}
static inline Operand parse_op_val(Program * program, const char ** cursor)
{
    return new_op_val(parse_value(program, find_next_token(cursor)));
}
static inline Operand parse_op_text(const char ** cursor)
{
    const char * label_name = strcpy_z(find_next_token(cursor));
    return new_op_text(label_name);
}

static inline uint8_t op_is_v(const char * opname)
{
    const char * ops[] = {
        "bnot", "not", "bool", "neg", "f32_to_f64", "f64_to_f32", "freeze", "ptralias_bless", "mov"
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(char *); i++)
    {
        if (strcmp(opname, ops[i]) == 0)
            return 1;
    }
    return 0;
}

static inline uint8_t op_is_t_v(const char * opname)
{
    const char * ops[] = {
        "load", "trim", "qext", "zext", "sext",
        "float_to_uint", "float_to_uint_unsafe", "uint_to_float",
        "float_to_sint", "float_to_sint_unsafe", "sint_to_float",
        "bitcast", 
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(char *); i++)
    {
        if (strcmp(opname, ops[i]) == 0)
            return 1;
    }
    return 0;
}

static inline uint8_t op_is_v_v(const char * opname)
{
    const char * ops[] = {
        "add", "sub", "mul", "imul", "div", "idiv", "rem", "irem", "div_unsafe", "idiv_unsafe", "rem_unsafe", "irem_unsafe",
        "shl", "shr", "shr_unsafe", "sar", "sar_unsafe", "and", "or", "xor",
        "cmp_eq", "cmp_ne", "cmp_ge", "cmp_le", "cmp_g", "cmp_l",
        "icmp_ge", "icmp_le", "icmp_g", "icmp_l",
        
        "fadd", "fsub", "fmul", "fdiv", "frem",
        "fxor",
        "fcmp_eq", "fcmp_ne", "fcmp_ge", "fcmp_le", "fcmp_g", "fcmp_l",
        
        "ptralias", "ptralias_merge", "ptralias_disjoint",
    };
    for (size_t i = 0; i < sizeof(ops)/sizeof(char *); i++)
    {
        if (strcmp(opname, ops[i]) == 0)
            return 1;
    }
    return 0;
}

static inline uint8_t op_is_v_v_v(const char * opname)
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

static inline Statement * parse_statement(Program * program, const char ** cursor)
{
    Statement * ret = new_statement();
    char * token = strcpy_z(find_next_token(cursor));
    const char * cursor_before_token2 = *cursor;
    char * token2 = find_next_token(cursor);
    
    if (token2 && strcmp("=", token2) == 0)
    {
        assert_no_redefinition(program->current_func, program->current_block, token);
        
        ret->output_name = token;
        char * token_1 = strcpy_z(find_next_token(cursor));
        ret->statement_name = token_1;
        
        if (op_is_v_v(ret->statement_name))
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            const char * op2_text = strcpy_z(find_next_token(cursor));
            
            Operand op1 = parse_op_val(program, &op1_text);
            Operand op2 = parse_op_val(program, &op2_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, op2);
            
            return ret;
        }
        else if (op_is_v(ret->statement_name))
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            Operand op1 = parse_op_val(program, &op1_text);
            array_push(ret->args, Operand, op1);
            return ret;
        }
        else if (op_is_t_v(ret->statement_name))
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            const char * op2_text = strcpy_z(find_next_token(cursor));
            
            Operand op1 = parse_op_type(&op1_text);
            Operand op2 = parse_op_val(program, &op2_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, op2);
            
            return ret;
        }
        else if (strcmp(ret->statement_name, "symbol_lookup_unsized") == 0)
        {
            Operand op1 = parse_op_text(cursor);
            array_push(ret->args, Operand, op1);
            return ret;
        }
        else if (strcmp(ret->statement_name, "symbol_lookup") == 0)
        {
            Operand op1 = parse_op_text(cursor);
            const char * op2_text = strcpy_z(find_next_token(cursor));
            uint64_t size = parse_int_bare(op2_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, new_op_rawint(size));
            return ret;
        }
        else if (strcmp(ret->statement_name, "call_eval") == 0)
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            Operand op1 = parse_op_type(&op1_text);
            array_push(ret->args, Operand, op1);
            
            const char * op_text = find_next_token(cursor);
            uint8_t first = 1;
            while (op_text)
            {
                const char * text_copy = strcpy_z(op_text);
                Operand op = parse_op_val(program, &text_copy);
                assert(op.value);
                if (first)
                    assert(op.value->type.variant == TYPE_IPTR);
                array_push(ret->args, Operand, op);
                op_text = find_next_token(cursor);
                first = 0;
            }
            return ret;
        }
        else
        {
            printf("culprit: %s\n", ret->statement_name);
            assert(((void)"glfy39", 0));
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
                Operand op = new_op_val(parse_value(program, find_next_token(cursor)));
                array_push(ret->args, Operand, op);
            }
        }
        else if (strcmp(ret->statement_name, "breakpoint") == 0)
        {
            // (no arguments)
        }
        else if (strcmp(ret->statement_name, "store") == 0)
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            const char * op2_text = strcpy_z(find_next_token(cursor));
            
            Operand op1 = parse_op_val(program, &op1_text);
            Operand op2 = parse_op_val(program, &op2_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, op2);
            
            return ret;
        }
        else if (strcmp(ret->statement_name, "if") == 0)
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            const char * op2_text = strcpy_z(find_next_token(cursor));
            assert(strcmp(op2_text, "goto") == 0);
            const char * op3_text = strcpy_z(find_next_token(cursor));
            
            Operand op1 = parse_op_val(program, &op1_text);
            Operand op3 = parse_op_text(&op3_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, op3);
            
            const char * next = find_next_token(cursor);
            while (next && strcmp(next, "") != 0 && strcmp(next, "else") != 0)
            {
                next = strcpy_z(next);
                Operand op = parse_op_val(program, &next);
                array_push(ret->args, Operand, op);
                next = find_next_token(cursor);
            }
            
            if (next && strcmp(next, "else") == 0)
            {
                Operand op = new_op_separator();
                array_push(ret->args, Operand, op);
                
                const char * op2_text = strcpy_z(find_next_token(cursor));
                assert(op2_text);
                Operand op2 = parse_op_text(&op2_text);
                array_push(ret->args, Operand, op2);
                
                const char * next = find_next_token(cursor);
                while (next && strcmp(next, "") != 0)
                {
                    next = strcpy_z(next);
                    Operand op = parse_op_val(program, &next);
                    array_push(ret->args, Operand, op);
                    next = find_next_token(cursor);
                }
            }
            
            return ret;
        }
        else if (strcmp(ret->statement_name, "goto") == 0)
        {
            const char * op1_text = strcpy_z(find_next_token(cursor));
            Operand op1 = parse_op_text(&op1_text);
            array_push(ret->args, Operand, op1);
            
            const char * next = find_next_token(cursor);
            while (next && strcmp(next, "") != 0)
            {
                next = strcpy_z(next);
                Operand op = parse_op_val(program, &next);
                array_push(ret->args, Operand, op);
                next = find_next_token(cursor);
            }
            
            return ret;
        }
        // TODO: other statements
        else
        {
            printf("culprit: %s\n", ret->statement_name);
            assert(((void)"unknown instruction", 0));
        }
    }
    
    return ret;
}

static inline void legalize_last_statement_operands(Block * block)
{
    size_t end = array_len(block->statements, Statement *);
    if (end == 0)
        return;
    //size_t orig_end = end;
    Statement * statement = block->statements[end - 1];
    // pre-process any stack slot address arguments into explicit address access instructions
    if (strcmp(statement->statement_name, "load") != 0 &&
        strcmp(statement->statement_name, "store") != 0 &&
        strcmp(statement->statement_name, "mov") != 0)
    {
        for (size_t i = 0; i < array_len(statement->args, Operand); i++)
        {
            Value * value = statement->args[i].value;
            if (!value)
                continue;
            if (value->variant == VALUE_STACKADDR)
            {
                Statement * s = new_statement();
                char * str = make_temp_name();
                s->output_name = str;
                s->statement_name = strcpy_z("mov");
                s->block = block;
                
                // move around args
                array_push(s->args, Operand, statement->args[i]);
                
                add_statement_output(s);
                Operand op = new_op_val(s->output);
                statement->args[i] = op;
                
                // swap locations
                block->statements[end - 1] = s;
                array_push(block->statements, Statement *, statement);
                
                // update "end"
                end = array_len(block->statements, Statement *);
            }
        }
    }
    
    // convert constants into registers if needed
    
    ImmOpsAllowed rules = imm_op_rule_determiner(statement);
    assert(sizeof(rules.immediates_allowed[0]) == 1);
    
    for (size_t i = 0; i < 8 && i < array_len(statement->args, Operand); i++)
    {
        uint8_t imm_allowed = rules.immediates_allowed[i];
        if (imm_allowed)
            continue;
        
        Value * value = statement->args[i].value;
        if (!value)
            continue;
        if (value->variant != VALUE_CONST)
            continue;
        
        Statement * s = new_statement();
        char * str = make_temp_name();
        s->output_name = str;
        s->statement_name = strcpy_z("mov");
        s->block = block;
        
        // move around args
        array_push(s->args, Operand, statement->args[i]);
        
        add_statement_output(s);
        Operand op = new_op_val(s->output);
        statement->args[i] = op;
        
        // swap locations
        block->statements[end - 1] = s;
        array_push(block->statements, Statement *, statement);
        
        // update "end"
        end = array_len(block->statements, Statement *);
    }
}

/// @brief Add a statement to the end of a block, creating the statement's output value and linking the statement back to the block.
/// @param block 
/// @param statement 
static inline void block_append_statement(Block * block, Statement * statement)
{
    add_statement_output(statement);
    //add_statement(program, statement);
    
    statement->block = block;
    array_push(block->statements, Statement *, statement);
    
    legalize_last_statement_operands(block);
}

static inline Statement * parse_and_add_statement(Program * program, const char ** cursor)
{
    Statement * statement = parse_statement(program, cursor);
    block_append_statement(program->current_block, statement);
    return statement;
}

/// @brief Add an argument with a given name and type to the current function. Panics if name is already in use. If name is null, generates one.
/// @param func 
/// @param name 
/// @param type 
/// @return
static inline Value * add_funcarg(Function * func, const char * name, Type type)
{
    if (!name)
        name = make_temp_name();
    else
        name = strcpy_z(name);
    
    assert_no_redefinition(func, 0, name);
    
    Value * value = make_value(type);
    value->variant = VALUE_ARG;
    value->arg = name;
    //printf("creating func arg with name %s\n", name);
    
    array_push(func->args, Value *, value);
    
    return value;
}

/// @brief Add an argument with a given name and type to the current block. Panics if name is already in use. If name is null, generates one.
/// @param program 
/// @param name 
/// @param type 
/// @return 
static inline Value * add_blockarg(Program * program, const char * name, Type type)
{
    if (!name)
        name = make_temp_name();
    
    assert_no_redefinition(program->current_func, program->current_block, name);
    
    Value * value = make_value(type);
    value->variant = VALUE_ARG;
    value->arg = name;
    //printf("creating block arg with name %s\n", name);
    
    array_push(program->current_block->args, Value *, value);
    
    return value;
}

/// @brief Adds a stack slot with a given name and size to the given function. Panics if name is already in use. If name is null, generates one.
/// @param func Function to which the stack slot belongs.
/// @param name Name of stack slot.
/// @param size Size in bytes of stack slot.
/// @return 
static inline Value * add_stack_slot(Function * func, const char * name, uint64_t size)
{
    if (!name)
        name = make_temp_name();
    assert_no_redefinition(func, 0, name);
    StackSlot _slot = {strcpy_z(name), size, 0, 0};
    StackSlot * slot = (StackSlot *)zero_alloc(sizeof(StackSlot));
    *slot = _slot;
    Value * val = make_stackslot_value(slot);
    array_push(func->stack_slots, Value *, val);
    return val;
}

/// @brief  Creates a block in the current function and switches to it. If name is null, generates one.
/// @param program 
/// @param name 
static inline Block * create_block(Program * program, const char * name)
{
    if (!name)
        name = make_temp_name();
    program->current_block = new_block();
    array_push(program->current_func->blocks, Block *, program->current_block);
    program->current_block->name = strcpy_z(name);
    return program->current_block;
}

/// @brief Creates a new function in the given program and switches to it. If name is null, generates one.
/// @param program 
/// @param name 
/// @param return_type 
/// @return 
static inline Function * create_function(Program * program, const char * name, Type return_type)
{
    if (!name)
        name = make_temp_name();
    program->current_func = new_func();
    program->current_func->name = strcpy_z(name);
    program->current_func->return_type = return_type;
    program->current_func->entry_block = create_block(program, "__entry__");
    array_push(program->functions, Function *, program->current_func);
    
    return program->current_func;
}

/// @brief  Initializes a statement object with the given operation or instruction name.
/// @param statement_name 
/// @return 
static inline Statement * init_statement(const char * statement_name)
{
    Statement * ret = new_statement();
    ret->statement_name = statement_name;
    return ret;
}

/// @brief Add a value as an operand/argument to the given statement.
/// @param statement
/// @param val 
static inline void statement_add_type_op(Statement * statement, Type type_)
{
    array_push(statement->args, Operand, new_op_type(type_));
}

/// @brief Add a value as an operand/argument to the given statement.
/// @param statement
/// @param val 
static inline void statement_add_value_op(Statement * statement, Value * val)
{
    array_push(statement->args, Operand, new_op_val(val));
}

/// @brief Add a string (label or symbol name, but not variable name) as an operand/argument to the given statement.
/// @param statement
/// @param val 
static inline void statement_add_text_op(Statement * statement, const char * text)
{
    array_push(statement->args, Operand, new_op_text(strcpy_z(text)));
}

/// @brief Creates a program with no functions, globals, or statics in it.
/// @param  
/// @return 
static inline Program * create_empty_program(void)
{
    Program * program = (Program *)zero_alloc(sizeof(Program));
    program->functions = (Function **)zero_alloc(0);
    program->globals = (GlobalData *)zero_alloc(0);
    program->statics = (StaticData *)zero_alloc(0);
    program->unused_relocation_log = (UnusedRelocation *)zero_alloc(0);
    return program;
}

static inline Program * parse_file(const char * cursor)
{
    Program * program = create_empty_program();
    
    enum BBAE_PARSER_STATE state = PARSER_STATE_ROOT;
    char * token = find_next_token_anywhere(&cursor);
    
    while (token)
    {
        if (state == PARSER_STATE_ROOT)
        {
            if (strcmp(token, "func") == 0)
            {
                token = find_next_token(&cursor);
                assert(token);
                const char * name = strcpy_z(token);
                
                token = find_next_token(&cursor);
                Type return_type = basic_type(TYPE_NONE);
                if (token && strcmp(token, "returns") == 0)
                    return_type = parse_type(&cursor);
                
                create_function(program, name, return_type);
                
                state = PARSER_STATE_FUNCARGS;
            }
            else if (strcmp(token, "global") == 0)
            {
                Type type = parse_type(&cursor);
                token = find_next_token(&cursor);
                assert(token);
                const char * name = strcpy_z(token);
                
                add_global(program, name, type, 0);
            }
            else if (strcmp(token, "static") == 0)
            {
                assert(((void)"TODO static", 0));
            }
            else
            {
                printf("culprit: %s\n", token);
                assert(((void)"unknown directive name", 0));
            }
        }
        else if (state == PARSER_STATE_BLOCKARGS)
        {
            if (strcmp(token, "arg") == 0)
            {
                token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_z(token);
                
                Type type = parse_type(&cursor);
                
                add_blockarg(program, name, type);
            }
            else
            {
                state = PARSER_STATE_BLOCK;
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
                
                add_funcarg(program->current_func, name, type);
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
                
                add_stack_slot(program->current_func, name, size);
            }
            else
            {
                state = PARSER_STATE_BLOCK;
                continue;
            }
        }
        else if (state == PARSER_STATE_BLOCK)
        {
            if (strcmp(token, "block") != 0 && strcmp(token, "endfunc") != 0)
            {
                cursor -= strlen(token);
                
                parse_and_add_statement(program, &cursor);
            }
            else if (strcmp(token, "block") == 0)
            {
                token = find_next_token(&cursor);
                assert(token);
                const char * name = strcpy_z(token);
                
                create_block(program, name);
                
                state = PARSER_STATE_BLOCKARGS;
            }
            else if (strcmp(token, "endfunc") == 0)
            {
                program->current_func = 0;
                state = PARSER_STATE_ROOT;
            }
            else
            {
                printf("culprit: %s\n", token);
                assert(((void)"unknown statement or directive", 0));
            }
        }
        
        find_end_of_line(&cursor);
        token = find_next_token_anywhere(&cursor);
    }
    
    //puts("finished parsing program!");
    
    return program;
}

// split blocks if they have any conditional jumps
static inline void split_blocks(Program * program)
{
    // edges aren't connected yet, so we don't have to repair them
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            
            // collect which SSA values are actually needed by which latest instructions
            // so that we can pass along the right arguments to the secondary blocks
            const char ** output_names = (const char **)zero_alloc(0);
            Value ** outputs = (Value **)zero_alloc(0);
            int64_t * output_latest_use = (int64_t *)zero_alloc(0);
            
            Value ** args = (block == func->entry_block) ? func->args : block->args;
            
            // collect arg live ranges
            for (size_t i = 0; i < array_len(args, Value *); i++)
            {
                Value * arg = args[i];
                assert(arg->variant == VALUE_ARG);
                arg->temp = array_len(output_latest_use, uint64_t);
                array_push(output_names, const char *, arg->arg);
                array_push(outputs, Value *, arg);
                array_push(output_latest_use, int64_t, -1);
            }
            
            // find branches, if any, while collecting SSA var live ranges
            uint8_t branch_found = 0;
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                
                if (statement->output && !branch_found)
                {
                    assert(statement->output_name);
                    assert(statement->output->variant == VALUE_SSA);
                    statement->output->temp = array_len(output_latest_use, uint64_t);
                    array_push(output_names, const char *, statement->output_name);
                    array_push(outputs, Value *, statement->output);
                    array_push(output_latest_use, int64_t, -1);
                }
                
                if (strcmp(statement->statement_name, "if") == 0)
                {
                    if (find_separator_index(statement->args) == (size_t)-1)
                        branch_found = 1;
                }
                
                for (size_t j = 0; j < array_len(statement->args, Operand); j++)
                {
                    Value * arg = statement->args[j].value;
                    if (arg && (arg->variant == VALUE_ARG || arg->variant == VALUE_SSA))
                    {
                        uint64_t k = arg->temp;
                        if (outputs[k] == arg)
                            output_latest_use[k] = i;
                    }
                }
            }
            
            if (!branch_found)
                continue;
            
            // finally, actually split the block
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * branch = block->statements[i];
                if (strcmp(branch->statement_name, "if") == 0)
                {
                    Block * next_block = new_block();
                    next_block->name = make_temp_name();
                    next_block->statements = array_chop(block->statements, Statement *, i+1);
                    for (size_t s = 0; s < array_len(next_block->statements, Statement *); s++)
                        next_block->statements[s]->block = next_block;
                    
                    //printf("splitting block %s at instruction %zu\n", block->name, i);
                    //printf("left len: %zu\n", array_len(block->statements, Statement *));
                    //printf("right len: %zu\n", array_len(next_block->statements, Statement *));
                    
                    array_push(branch->args, Operand, new_op_separator());
                    array_push(branch->args, Operand, new_op_text(next_block->name));
                    
                    for (size_t a = 0; a < array_len(output_latest_use, uint64_t); a++)
                    {
                        Value * value = outputs[a];
                        if (output_latest_use[a] <= (int64_t)i)
                        {
                            //printf("--- skipping rewriting operand %s because %zu vs %zu\n", value->ssa ? value->ssa->output_name : value->arg, output_latest_use[a], i);
                            continue;
                        }
                        //printf("--- YES rewriting operand %s because %zu vs %zu\n", value->ssa ? value->ssa->output_name : value->arg, output_latest_use[a], i);
                        
                        Value * arg = make_value(value->type);
                        arg->variant = VALUE_ARG;
                        //printf("creating dummy block arg with name %s\n", output_names[a]);
                        arg->arg = output_names[a];
                        
                        array_push(next_block->args, Value *, arg);
                        array_push(branch->args, Operand, new_op_val(value));
                        for (size_t j = 0; j < array_len(next_block->statements, Statement *); j++)
                        {
                            Statement * statement = next_block->statements[j];
                            for (size_t n = 0; n < array_len(statement->args, Operand); n++)
                            {
                                if (statement->args[n].variant == OP_KIND_VALUE && statement->args[n].value == value)
                                {
                                    //printf("replaced a usage of %s in statement type %s\n", output_names[a], statement->statement_name);
                                    disconnect_statement_from_operand(statement, statement->args[n], 1);
                                    statement->args[n].value = arg;
                                    connect_statement_to_operand(statement, statement->args[n]);
                                }
                            }
                        }
                    }
                    array_insert(func->blocks, Block *, b + 1, next_block);
                    
                    break;
                }
            }
        }
    }
}

static inline void block_edges_disconnect(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 1; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            while (array_len(block->edges_in, Statement *) > 0)
                array_erase(block->edges_in, Statement *, array_len(block->edges_in, Statement *) - 1);
        }
    }
}

static inline void block_edges_connect(Program * program)
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
                assert(statement->statement_name);
                if (strcmp(statement->statement_name, "goto") == 0)
                {
                    assert(statement->args[0].text);
                    assert(statement->block);
                    Block * target = find_block(func, statement->args[0].text);
                    assert(target);
                    array_push(target->edges_in, Statement *, statement);
                }
                if (strcmp(statement->statement_name, "if") == 0)
                {
                    assert(statement->args[1].text);
                    assert(statement->block);
                    Block * target = find_block(func, statement->args[1].text);
                    assert(target);
                    array_push(target->edges_in, Statement *, statement);
                    
                    size_t separator_pos = find_separator_index(statement->args);
                    // block splitting is required to have happened before now
                    assert(separator_pos != (size_t)-1);
                    
                    assert(statement->args[separator_pos + 1].text);
                    assert(statement->block);
                    target = find_block(func, statement->args[separator_pos + 1].text);
                    assert(target);
                    array_push(target->edges_in, Statement *, statement);
                }
            }
        }
    }
}

static inline void block_statements_connect(Program * program)
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
                for (size_t n = 0; n < array_len(statement->args, Operand); n++)
                    connect_statement_to_operand(statement, statement->args[n]);
                
            }
        }
    }
}

static inline void program_finish_construction(Program * program)
{
    split_blocks(program);
    block_statements_connect(program);
    block_edges_connect(program);
    program->construction_finished = 1;
}

#endif // BBAE_CONSTRUCTION
