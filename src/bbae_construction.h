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

static Value * parse_value(Program * program, char * token)
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
                 || str_ends_with(token, "i32") || str_ends_with(token, "i64")
                 || str_ends_with(token, "iptr"))
        {
            uint64_t n = parse_int_nonbare(token);
            printf("parsed int... %zd\n", n);
            ret->variant = VALUE_CONST;
            if (str_ends_with(token, "i8"))
                ret->type = basic_type(TYPE_I8);
            else if (str_ends_with(token, "i16"))
                ret->type = basic_type(TYPE_I16);
            else if (str_ends_with(token, "i32"))
                ret->type = basic_type(TYPE_I32);
            else if (str_ends_with(token, "i64"))
                ret->type = basic_type(TYPE_I64);
            else if (str_ends_with(token, "iptr"))
                ret->type = basic_type(TYPE_IPTR);
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
                    assert(("tried to use output of operation before it was run", 0));
                }
                return statement->output;
            }
        }
        
        printf("culprit: %s\n", token);
        assert(("tried to use unknown variable", 0));
    }
}

static Operand parse_op_val(Program * program, const char ** cursor)
{
    return new_op_val(parse_value(program, find_next_token(cursor)));
}


static uint8_t op_is_v(char * opname)
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

static uint8_t op_is_v_v(char * opname)
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

static uint8_t op_is_v_v_v(char * opname)
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

static Statement * parse_statement(Program * program, const char ** cursor)
{
    Statement * ret = new_statement();
    char * token = strcpy_z(find_next_token(cursor));
    const char * cursor_before_token2 = *cursor;
    char * token2 = find_next_token(cursor);
    
    if (token2 && strcmp("=", token2) == 0)
    {
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
                Operand op = new_op_val(parse_value(program, find_next_token(cursor)));
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

static void fix_last_statement_stack_slot_addr_usage(Block * block)
{
    size_t end = array_len(block->statements, Statement *);
    if (end == 0)
        return;
    Statement * statement = block->statements[end - 1];
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
                Operand op = new_op_val(s->output);
                statement->args[i] = op;
                
                // swap IDs
                /*
                uint64_t temp_id = s->id;
                s->id = statement->id;
                statement->id = temp_id;
                */
                
                // swap locations
                block->statements[end - 1] = s;
                array_push(block->statements, Statement *, statement);
                
                // update "end"
                end = array_len(block->statements, Statement *);
            }
        }
    }
}

static Program * parse_file(const char * cursor)
{
    Program * program = (Program *)zero_alloc(sizeof(Program));
    program->functions = (Function **)zero_alloc(0);
    program->current_func = new_func();
    
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
                
                program->current_func->name = strcpy_z(token);
                
                token = find_next_token(&cursor);
                if (token && strcmp(token, "returns"))
                    program->current_func->return_type = parse_type(&cursor);
                
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
                array_push(program->current_func->args, Value *, value);
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
                program->current_block = new_block();
                array_push(program->current_func->blocks, Block *, program->current_block);
                program->current_func->entry_block = program->current_block;
                
                state = PARSER_STATE_BLOCK;
                continue;
            }
        }
        else if (state == PARSER_STATE_BLOCK)
        {
            if (strcmp(token, "block") != 0 && strcmp(token, "endfunc") != 0)
            {
                cursor -= strlen(token);
                Statement * statement = parse_statement(program, &cursor);
                add_statement_output(statement);
                array_push(program->current_block->statements, Statement *, statement);
                fix_last_statement_stack_slot_addr_usage(program->current_block);
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
                array_push(program->functions, Function *, program->current_func);
                program->current_func = new_func();
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
    
    return program;
}

// split blocks if they have any conditional jumps
void split_blocks(Program * program)
{
    // TODO: do a pre-pass collecting the final use of each variable/argument.
    // This will be redundant work vs the graph connection pass, but has to happen separately
    // because the graph connection pass would point to things from the previous block
    // with the later block needing to be patched intrusively. This is simpler.
    
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
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

void connect_graphs(Program * program)
{
    split_blocks(program);
    
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

#endif // BBAE_CONSTRUCTION
