#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#define BBAE_DEBUG_SPILLS

#include "memory.h"
#include "emitter.h"
#include "abi.h"
#include "compiler_common.h"
#include "regalloc_x86.h"
#include "jitify.h"

enum BBAE_PARSER_STATE {
    PARSER_STATE_ROOT,
    PARSER_STATE_FUNCARGS,
    PARSER_STATE_FUNCSLOTS,
    PARSER_STATE_BLOCKARGS,
    PARSER_STATE_BLOCK,
};

Value * parse_value(Program * program, char * token)
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
                 || str_ends_with(token, "i32") || str_ends_with(token, "i64"))
        {
            //uint8_t suffix_len = str_ends_with(token, "i8") ? 2 : 3;
            uint64_t n = parse_int_nonbare(token);
            printf("parsed int... %lld\n", n);
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

Operand parse_op_val(Program * program, char ** cursor)
{
    return new_op_val(parse_value(program, find_next_token(cursor)));
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

Statement * parse_statement(Program * program, char ** cursor)
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
            
            Operand op1 = parse_op_val(program, &op1_text);
            Operand op2 = parse_op_val(program, &op2_text);
            array_push(ret->args, Operand, op1);
            array_push(ret->args, Operand, op2);
            
            return ret;
        }
        else if (op_is_v(ret->statement_name))
        {
            char * op1_text = strcpy_z(find_next_token(cursor));
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

void fix_last_statement_stack_slot_addr_usage(Block * block)
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

Program * parse_file(char * cursor)
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

void allocate_stack_slots(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
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

byte_buffer * code;

EncOperand get_basic_encoperand(Value * value)
{
    assert(value->variant == VALUE_CONST || value->variant == VALUE_STACKADDR || value->regalloced);
    if (value->variant == VALUE_CONST)
        return zy_imm(value->constant);
    else if (value->variant == VALUE_SSA || value->variant == VALUE_ARG)
        return zy_reg(value->regalloc, type_size(value->type));
    else if (value->variant == VALUE_STACKADDR)
        return zy_mem(REG_RBP, -value->slotinfo->offset, value->slotinfo->size);
    else
    {
        printf("culprit: %d\n", value->variant);
        assert(("TODO", 0));
    }
}

void compile_file(Program * program)
{
    code = (byte_buffer *)malloc(sizeof(byte_buffer));
    memset(code, 0, sizeof(byte_buffer));
    
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
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
                    
                    EncOperand op1 = zy_reg(REG_RAX, type_size(op.value->type));
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
                else if (strcmp(statement->statement_name, "load") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    
                    zy_emit_2(code, INST_MOV, op0, op1);
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

void print_asm(void)
{
    //FILE * logfile = 0;
    
    size_t offset = 0; 
    size_t runtime_address = 0; 
    ZydisDisassembledInstruction instruction; 
    while (ZYAN_SUCCESS(ZydisDisassembleIntel(
        ZYDIS_MACHINE_MODE_LONG_64,
        runtime_address,
        code->data + offset,  // buffer
        code->len - offset,   // length
        &instruction
    )))
    {
        printf("    %s\n", instruction.text);
        offset += instruction.info.length;
        runtime_address += instruction.info.length;
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
    
    // parsing and configuration
    Program * program = parse_file(buffer);
    connect_graphs(program);
    
    // TODO: optimization goes here
    
    // lowering
    do_regalloc(program);
    allocate_stack_slots(program);
    compile_file(program);
    
    for (size_t i = 0; i < code->len; i++)
        printf("%02X ", code->data[i]);
    puts("");
    
    uint8_t * jit_code = copy_as_executable(code->data, code->len);
    
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    int (*jit_main) (int, int) = (int(*)(int, int))(void *)(jit_code);
#pragma GCC diagnostic pop
    
    assert(jit_main);
    uint32_t asdf = jit_main(5, 5);
    
    printf("output: %d (0x%X)\n", asdf, asdf);
    
    print_asm();
    
    free_as_executable(jit_code);
    free_all_compiler_allocs();
    
    return 0;
}
