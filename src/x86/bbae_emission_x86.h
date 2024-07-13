#ifndef BBAE_EMISSION
#define BBAE_EMISSION

#include "regalloc_x86.h"
#include "emitter_x86.h"
#include "../compiler_common.h"

static void allocate_stack_slots(Program * program)
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

static EncOperand get_basic_encoperand(Value * value)
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

static byte_buffer * compile_file(Program * program)
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

#endif // BBAE_EMISSION
