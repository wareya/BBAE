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

#include "bbae_api.h"

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

byte_buffer * compile_file(Program * program)
{
    byte_buffer * code = (byte_buffer *)malloc(sizeof(byte_buffer));
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
    
    return code;
}

void print_asm(byte_buffer * code)
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
    
    Program * program = parse(buffer);
    
    // parsing and configuration
    // TODO: optimization goes here
    
    // lowering
    do_regalloc(program);
    allocate_stack_slots(program);
    byte_buffer * code = compile_file(program);
    
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
    
    print_asm(code);
    
    free_as_executable(jit_code);
    free_all_compiler_allocs();
    
    free(buffer);
    
    return 0;
}
