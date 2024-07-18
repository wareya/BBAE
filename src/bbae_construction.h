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

uint8_t check_for_redefinition(Program * program, char * name)
{
    Function * func = program->current_func;
    Block * block = program->current_block;
    
    assert(func);
    
    assert(block == func->entry_block || block);
    Value ** args = (block == func->entry_block) ? func->args : block->args;
    
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

void assert_no_redefinition(Program * program, char * name)
{
    if (check_for_redefinition(program, name))
    {
        printf("culprit: %s\n", name);
        assert(((void)"variable redefined!", 0));
    }
}

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
            assert(((void)"invalid float literal", strlen(token) - (size_t)(end - token) == 3));
            ret->variant = VALUE_CONST;
            ret->type = basic_type(TYPE_F32);
            memcpy(&ret->constant, &f, 4);
        }
        else if (str_ends_with(token, "f64"))
        {
            char * end = 0;
            double f = strtod(token, &end);
            assert(((void)"invalid float literal", strlen(token) - (size_t)(end - token) == 3));
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
            assert(((void)"unknown type of literal value", 0));
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
                    assert(((void)"tried to use output of operation before it was run", 0));
                }
                return statement->output;
            }
        }
        
        printf("culprit: %s\n", token);
        assert(((void)"tried to use unknown variable", 0));
    }
}

static Operand parse_op_type(const char ** cursor)
{
    return new_op_type(parse_type(cursor));
}
static Operand parse_op_val(Program * program, const char ** cursor)
{
    return new_op_val(parse_value(program, find_next_token(cursor)));
}
static Operand parse_op_text(const char ** cursor)
{
    const char * label_name = strcpy_z(find_next_token(cursor));
    return new_op_text(label_name);
}


static uint8_t op_is_v(const char * opname)
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

static uint8_t op_is_t_v(const char * opname)
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

static uint8_t op_is_v_v(const char * opname)
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

static uint8_t op_is_v_v_v(const char * opname)
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
        assert_no_redefinition(program, token);
        
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

static void legalize_last_statement_operands(Block * block)
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

static Program * parse_file(const char * cursor)
{
    Program * program = (Program *)zero_alloc(sizeof(Program));
    program->functions = (Function **)zero_alloc(0);
    program->statics = (StaticData *)zero_alloc(0);
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
                assert(((void)"TODO global", 0));
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
                puts("found block args");
                
                token = find_next_token(&cursor);
                assert(token);
                char * name = strcpy_z(token);
                Type type = parse_type(&cursor);
                
                assert_no_redefinition(program, name);
                
                Value * value = make_value(type);
                value->variant = VALUE_ARG;
                value->arg = name;
                printf("creating block arg with name %s\n", name);
                array_push(program->current_block->args, Value *, value);
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
                
                assert_no_redefinition(program, name);
                
                Value * value = make_value(type);
                value->variant = VALUE_ARG;
                value->arg = name;
                printf("creating func arg with name %s\n", name);
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
                
                assert_no_redefinition(program, name);
                
                token = find_next_token(&cursor);
                uint64_t size = parse_int_bare(token);
                
                add_stack_slot(program->current_func, name, size);
            }
            else
            {
                program->current_block = new_block();
                array_push(program->current_func->blocks, Block *, program->current_block);
                program->current_block->name = "__entry__";
                program->current_func->entry_block = program->current_block;
                puts("---- added entry block");
                printf("(state: %d)\n", state);
                
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
                statement->block = program->current_block;
                array_push(program->current_block->statements, Statement *, statement);
                legalize_last_statement_operands(program->current_block);
            }
            else if (strcmp(token, "block") == 0)
            {
                program->current_block = new_block();
                array_push(program->current_func->blocks, Block *, program->current_block);
                
                token = find_next_token(&cursor);
                assert(token);
                program->current_block->name = strcpy_z(token);
                
                state = PARSER_STATE_BLOCKARGS;
            }
            else if (strcmp(token, "endfunc") == 0)
            {
                array_push(program->functions, Function *, program->current_func);
                program->current_func = new_func();
                // end of function
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
    
    puts("finished parsing program!");
    
    return program;
}

// split blocks if they have any conditional jumps
void split_blocks(Program * program)
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
            uint64_t * output_latest_use = (uint64_t *)zero_alloc(0);
            
            Value ** args = (block == func->entry_block) ? func->args : block->args;
            
            for (size_t i = 0; i < array_len(args, Value *); i++)
            {
                Value * arg = args[i];
                assert(arg->variant == VALUE_ARG);
                arg->temp = array_len(output_latest_use, uint64_t);
                array_push(output_names, const char *, arg->arg);
                array_push(outputs, Value *, arg);
                array_push(output_latest_use, uint64_t, (uint64_t)(int64_t)-1);
            }
            
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
                    array_push(output_latest_use, uint64_t, (uint64_t)(int64_t)-1);
                }
                
                if (strcmp(statement->statement_name, "if") == 0)
                    branch_found = 1;
                
                for (size_t j = 0; j < array_len(statement->args, Operand); j++)
                {
                    Value * arg = statement->args[j].value;
                    if (arg && (arg->variant == VALUE_ARG || arg->variant == VALUE_SSA))
                    {
                        uint64_t k = arg->temp;
                        //assert(outputs[k] == arg);
                        if (outputs[k] == arg)
                            output_latest_use[k] = i;
                    }
                }
            }
            
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * branch = block->statements[i];
                if (strcmp(branch->statement_name, "if") == 0)
                {
                    Block * next_block = new_block();
                    next_block->name = make_temp_name();
                    next_block->statements = array_chop(block->statements, Statement *, i+1);
                    
                    printf("splitting block %s at instruction %zu\n", block->name, i);
                    printf("left len: %zu\n", array_len(block->statements, Statement *));
                    printf("right len: %zu\n", array_len(next_block->statements, Statement *));
                    
                    array_push(branch->args, Operand, new_op_separator());
                    array_push(branch->args, Operand, new_op_text(next_block->name));
                    
                    for (size_t a = 0; a < array_len(output_latest_use, uint64_t); a++)
                    {
                        if (output_latest_use[a] <= i)
                            continue;
                        Value * arg = make_value(outputs[a]->type);
                        arg->variant = VALUE_ARG;
                        printf("creating dummy block arg with name %s\n", output_names[a]);
                        arg->arg = output_names[a];
                        array_push(next_block->args, Value *, arg);
                        array_push(branch->args, Operand, new_op_val(outputs[a]));
                        for (size_t j = 0; j < array_len(next_block->statements, Statement *); j++)
                        {
                            Statement * statement = next_block->statements[j];
                            for (size_t n = 0; n < array_len(statement->args, Operand); n++)
                            {
                                if (statement->args[n].variant == OP_KIND_VALUE && statement->args[n].value == outputs[a])
                                    statement->args[n].value = arg;
                            }
                        }
                    }
                    
                    array_insert(func->blocks, Block *, b + 1, next_block);
                    //array_push(next_block->edges_in, Statement *, branch);
                    
                    break;
                    //assert(((void)"TODO: split block, convey arguments", 0));
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
                
                if (statement->statement_name)
                {
                    if (strcmp(statement->statement_name, "goto") == 0)
                    {
                        assert(statement->args[0].text);
                        assert(statement->block);
                        Block * target = find_block(func, statement->args[0].text);
                        array_push(target->edges_in, Statement *, statement);
                        array_push(statement->block->edges_out, Statement *, statement);
                    }
                    if (strcmp(statement->statement_name, "if") == 0)
                    {
                        assert(statement->args[1].text);
                        assert(statement->block);
                        Block * target = find_block(func, statement->args[1].text);
                        array_push(target->edges_in, Statement *, statement);
                        array_push(statement->block->edges_out, Statement *, statement);
                        
                        size_t separator_pos = 0;
                        for (size_t i = 2; i < array_len(statement->args, Operand); i++)
                        {
                            if (statement->args[i].variant == OP_KIND_SEPARATOR)
                            {
                                separator_pos = i;
                                break;
                            }
                        }
                        // block splitting is required to have happened before now
                        assert(separator_pos);
                        
                        assert(statement->args[separator_pos + 1].text);
                        assert(statement->block);
                        target = find_block(func, statement->args[separator_pos + 1].text);
                        array_push(target->edges_in, Statement *, statement);
                        array_push(statement->block->edges_out, Statement *, statement);
                    }
                }
            }
        }
    }
}

#endif // BBAE_CONSTRUCTION
