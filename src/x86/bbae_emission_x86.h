#ifndef BBAE_EMISSION
#define BBAE_EMISSION

// Root file for the X86 backend, exposed to bbae_api.h.
// NOTE: Despite the name, this is a AMD64/x86_64 (64-bit only) backend. x86 is an architecture family, not a specific architecture.
// Other _x86 files

#include "regalloc_x86.h"
#include "emitter_x86.h"
#include "../compiler_common.h"
#include "../relocation_helpers.h"

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
        while (func->stack_height & 8)
            func->stack_height += 1;
        func->stack_height = offset;
    }
}

static EncOperand get_basic_encoperand_mem(Value * value, uint8_t want_ptr)
{
    assert(value->variant == VALUE_CONST || value->variant == VALUE_STACKADDR || value->regalloced);
    if (value->variant == VALUE_CONST)
    {
        assert(type_is_valid(value->type));
        assert(((void)"TODO", type_size(value->type) <= 8));
        return zy_imm(value->constant, type_size(value->type));
    }
    else if (value->variant == VALUE_SSA || value->variant == VALUE_ARG)
    {
        assert(type_is_valid(value->type));
        if (want_ptr)
        {
            assert(type_is_ptr(value->type));
            return zy_mem(value->regalloc, 0, type_size(value->type));
        }
        else
            return zy_reg(value->regalloc, type_size(value->type));
    }
    else if (value->variant == VALUE_STACKADDR)
    {
        //assert(want_ptr);
        return zy_mem(REG_RBP, -value->slotinfo->offset, value->slotinfo->size);
    }
    else
    {
        printf("culprit: %d\n", value->variant);
        assert(((void)"TODO", 0));
    }
}

static EncOperand get_basic_encoperand(Value * value)
{
    return get_basic_encoperand_mem(value, 0);
}

uint8_t reg_shuffle_needed(Value ** block_args, Operand * args, size_t count)
{
    return 1;
    for (size_t i = 0; i < count; i++)
    {
        assert(args[i].value);
        if (args[i].value->variant == VALUE_CONST)
            return 1;
        if (block_args[i]->regalloc != args[i].value->regalloc)
            return 1;
    }
    return 0;
}
void reg_shuffle_single(byte_buffer * code, int64_t * in2out, uint8_t * in2out_color, size_t in)
{
    int64_t out = in2out[in];
    EncOperand reg_scratch_int = zy_reg(REG_R11, 8);
    EncOperand reg_scratch_float = zy_reg(REG_XMM5, 8);
    
    EncOperand reg_out = zy_reg(out, 8);
    EncOperand reg_in = zy_reg(in, 8);
    
    if (in2out[out] < 0) // not a typo
    {
        //puts("a");
        if (in <= REG_R15)
            zy_emit_2(code, INST_MOV, reg_out, reg_in);
        else
            zy_emit_2(code, INST_MOVAPS, reg_out, reg_in);
        in2out[in] = -1;
    }
    else
    {
        // hit a cycle! mov into temp register. will be mov'd out of at the end of the cycle
        if (in2out_color[out])
        {
            //puts("b");
            if (out <= REG_R15)
            {
                zy_emit_2(code, INST_MOV, reg_scratch_int, reg_out);
                zy_emit_2(code, INST_MOV, reg_out, reg_in);
            }
            else
            {
                zy_emit_2(code, INST_MOVAPS, reg_scratch_float, reg_out);
                zy_emit_2(code, INST_MOVAPS, reg_out, reg_in);
            }
            
            in2out[in] = -1;
            in2out_color[out] = 2;
        }
        else
        {
            //puts("c");
            in2out_color[in] = 1;
            reg_shuffle_single(code, in2out, in2out_color, out);
            if (in2out_color[in] == 2)
            {
                //puts("d");
                if (in <= REG_R15)
                    zy_emit_2(code, INST_MOV, reg_out, reg_scratch_int);
                else
                    zy_emit_2(code, INST_MOVAPS, reg_out, reg_scratch_float);
            }
            else
            {
                //puts("e");
                if (in <= REG_R15)
                    zy_emit_2(code, INST_MOV, reg_out, reg_in);
                else
                    zy_emit_2(code, INST_MOVAPS, reg_out, reg_in);
            }
            in2out[in] = -1;
        }
    }
}

void do_reg_shuffle(byte_buffer * code, int64_t * in2out, uint8_t * in2out_color)
{
    //puts("----");
    for (size_t out = 0; out < 32; out++)
    {
        if (in2out[out] < 0)
            continue;
        reg_shuffle_single(code, in2out, in2out_color, out);
    }
}
void reg_shuffle_block_args(byte_buffer * code, Value ** block_args, Operand * args, size_t count)
{
    int64_t in2out[32];
    for (size_t i = 0; i < 32; i++)
        in2out[i] = -1;
    uint8_t in2out_color[32]; // for cycle detection
    memset(in2out_color, 0, sizeof(in2out_color));
    
    for (size_t i = 0; i < count; i++)
    {
        assert(args[i].value);
        if (args[i].value->variant == VALUE_CONST)
        {
            assert(((void)"FIXME handle const block args", 0));
            continue;
        }
        assert(args[i].value->variant == VALUE_SSA || args[i].value->variant == VALUE_ARG);
        
        assert(block_args[i]->regalloced);
        assert(((void)"spilled block args not yet supported", (int64_t)block_args[i]->regalloc >= 0));
        assert(block_args[i]->regalloc < 32);
        
        assert(args[i].value->regalloced);
        assert(((void)"spilled block args not yet supported", (int64_t)args[i].value->regalloc >= 0));
        assert(args[i].value->regalloc < 32);
        
        // no MOV needed
        if (block_args[i]->regalloc == args[i].value->regalloc)
            continue;
        
        in2out[args[i].value->regalloc] = block_args[i]->regalloc;
    }
    
    do_reg_shuffle(code, in2out, in2out_color);
}

void reg_shuffle_call(byte_buffer * code, Statement * call)
{
    int64_t in2out[32];
    for (size_t i = 0; i < 32; i++)
        in2out[i] = -1;
    uint8_t in2out_color[32]; // for cycle detection
    memset(in2out_color, 0, sizeof(in2out_color));
    
    size_t count = array_len(call->args, Operand);
    
    abi_reset_state();
    
    for (size_t i = 1; i < count; i++)
    {
        Value * value = call->args[i].value;
        assert(value);
        if (value->variant == VALUE_CONST)
        {
            assert(((void)"FIXME handle const call args", 0));
            continue;
        }
        assert(value->variant == VALUE_SSA || value->variant == VALUE_ARG);
        
        assert(value->regalloced);
        assert(((void)"spilled call args not yet supported",  (int64_t)value->regalloc >= 0));
        assert(value->regalloc < 32);
        assert(type_is_basic(value->type));
        
        int64_t where = abi_get_next_arg_basic(type_is_float(value->type));
        assert(((void)"on-stack call args not yet supported", where >= 0));
        
        // no MOV needed
        if (value->regalloc == (uint64_t)where)
            continue;
        
        in2out[value->regalloc] = where;
    }
    
    do_reg_shuffle(code, in2out, in2out_color);
}

static byte_buffer * compile_file(Program * program, SymbolEntry ** symbollist)
{
    byte_buffer * code = (byte_buffer *)zero_alloc(sizeof(byte_buffer));
    
    memset(code, 0, sizeof(byte_buffer));
    
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        
        if (code->len % 16)
            zy_emit_nops(code, 16 - (code->len % 16));
        
        SymbolEntry func_symbol;
        memset(&func_symbol, 0, sizeof(SymbolEntry));
        func_symbol.name = strcpy_z(func->name);
        func_symbol.loc = code->len;
        func_symbol.kind = 1; // function
        array_push(*symbollist, SymbolEntry, func_symbol);
        
        abi_get_callee_saved_regs(func->written_registers, 32);
        for (size_t i = 0; i < sizeof(func->written_registers); i++)
        {
            if (func->written_registers[i] == 2 && i != REG_RBP && i != REG_RSP)
                func->stack_height += 8;
        }
        while (func->stack_height & 16)
            func->stack_height += 1;
        
        EncOperand rbp = zy_reg(REG_RBP, 8);
        EncOperand rsp = zy_reg(REG_RSP, 8);
        zy_emit_1(code, INST_PUSH, rbp);
        zy_emit_2(code, INST_MOV, rbp, rsp);
        
        if (func->stack_height)
        {
            EncOperand height = zy_imm(func->stack_height, 4);
            zy_emit_2(code, INST_SUB, rsp, height);
            
            size_t n = 0;
            for (size_t i = 0; i < sizeof(func->written_registers); i++)
            {
                if (func->written_registers[i] == 2 && i != REG_RBP && i != REG_RSP)
                {
                    EncOperand mem = zy_mem(REG_RSP, n * 8, 8);
                    zy_emit_2(code, i >= REG_XMM0 ? INST_MOVQ : INST_MOV, mem, zy_reg(i, 8));
                    n += 1;
                }
            }
        }
        
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            Block * next_block = (b + 1 < array_len(func->blocks, Block *)) ? func->blocks[b + 1] : 0;
            
            block->start_offset = code->len;
            
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * prev_statement = i > 0 ? block->statements[i - 1] : 0;
                Statement * statement = block->statements[i];
                Statement * next_statement = (i + 1 < array_len(block->statements, Statement *)) ? block->statements[i + 1] : 0;
                
                if (strcmp(statement->statement_name, "return") == 0)
                {
                    assert(array_len(statement->args, Operand) == 0 || array_len(statement->args, Operand) == 1);
                    if (array_len(statement->args, Operand) == 1)
                    {
                        Operand op = statement->args[0];
                        assert(op.variant == OP_KIND_VALUE);
                        
                        EncOperand op2 = get_basic_encoperand(op.value);
                        if (op.value->type.variant == TYPE_F64 || op.value->type.variant == TYPE_F64)
                        {
                            EncOperand op1 = zy_reg(REG_XMM0, type_size(op.value->type));
                            if (!encops_equal(op1, op2))
                                zy_emit_2(code, INST_MOVQ, op1, op2);
                        }
                        else
                        {
                            EncOperand op1 = zy_reg(REG_RAX, type_size(op.value->type));
                            if (!encops_equal(op1, op2))
                                zy_emit_2(code, INST_MOV, op1, op2);
                        }
                    }
                    
                    size_t n = 0;
                    for (size_t i = 0; i < sizeof(func->written_registers); i++)
                    {
                        if (func->written_registers[i] == 2 && i != REG_RBP && i != REG_RSP)
                        {
                            EncOperand mem = zy_mem(REG_RSP, n * 8, 8);
                            zy_emit_2(code, i >= REG_XMM0 ? INST_MOVQ : INST_MOV, zy_reg(i, 8), mem);
                            n += 1;
                        }
                    }
                    
                    zy_emit_0(code, INST_LEAVE);
                    zy_emit_0(code, INST_RET);
                }
                else if (strcmp(statement->statement_name, "div") == 0 ||
                         strcmp(statement->statement_name, "idiv") == 0 ||
                         strcmp(statement->statement_name, "rem") == 0 ||
                         strcmp(statement->statement_name, "irem") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    assert(op1_op.value->regalloced);
                    assert(op2_op.value->regalloced);
                    
                    //EncOperand op0 = get_basic_encoperand(statement->output);
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    
                    if (type_size(statement->output->type) != 1)
                        zy_emit_2(code, INST_XOR, zy_reg(REG_RDX, 4), zy_reg(REG_RDX, 4));
                    
                    uint64_t forced_output = REG_RAX;
                    if (type_size(statement->output->type) != 1 && 
                        (strcmp(statement->statement_name, "rem") == 0 ||
                         strcmp(statement->statement_name, "irem") == 0))
                        forced_output = REG_RDX;
                    
                    assert(statement->output->regalloc == forced_output);
                    
                    if (op1_op.value->regalloc != REG_RAX)
                    {
                        EncOperand op1 = get_basic_encoperand(op1_op.value);
                        zy_emit_2(code, INST_MOV, zy_reg(REG_RAX, type_size(op1_op.value->type)), op1);
                    }
                    
                    if (strcmp(statement->statement_name, "div") == 0)
                        zy_emit_1(code, INST_DIV, op2);
                    else if (strcmp(statement->statement_name, "idiv") == 0)
                        zy_emit_1(code, INST_IDIV, op2);
                    else if (strcmp(statement->statement_name, "rem") == 0)
                    {
                        zy_emit_1(code, INST_DIV, op2);
                        //zy_emit_2(code, INST_SHR
                    }
                    else if (strcmp(statement->statement_name, "irem") == 0)
                    {
                        zy_emit_1(code, INST_IDIV, op2);
                    }
                    else
                        assert(((void)"FIXME handle more operations 1", 0));
                }
                else if (strcmp(statement->statement_name, "mul") == 0 ||
                         strcmp(statement->statement_name, "imul") == 0 ||
                         strcmp(statement->statement_name, "add") == 0 ||
                         strcmp(statement->statement_name, "sub") == 0 ||
                         strcmp(statement->statement_name, "shl") == 0 ||
                         strcmp(statement->statement_name, "shr") == 0 ||
                         
                         strcmp(statement->statement_name, "and") == 0 ||
                         strcmp(statement->statement_name, "or") == 0 ||
                         strcmp(statement->statement_name, "xor") == 0 ||
                         
                         strcmp(statement->statement_name, "fadd") == 0 ||
                         strcmp(statement->statement_name, "fsub") == 0 ||
                         strcmp(statement->statement_name, "fdiv") == 0 ||
                         strcmp(statement->statement_name, "fxor") == 0 ||
                         strcmp(statement->statement_name, "fmul") == 0)
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
                        if (strcmp(statement->statement_name, "add") == 0 ||
                            strcmp(statement->statement_name, "mul") == 0 ||
                            strcmp(statement->statement_name, "and") == 0 ||
                            strcmp(statement->statement_name, "or") == 0 ||
                            strcmp(statement->statement_name, "xor") == 0 ||
                            strcmp(statement->statement_name, "imul") == 0 ||
                            strcmp(statement->statement_name, "fadd") == 0 ||
                            strcmp(statement->statement_name, "fmul") == 0)
                        {
                            EncOperand temp = op1;
                            op1 = op2;
                            op2 = temp;
                        }
                        else
                            assert(((void)"FIXME handle more operations 2", 0));
                    }
                    
                    uint8_t shift_mov_performed = 0;
                    if (strcmp(statement->statement_name, "shl") == 0 ||
                        strcmp(statement->statement_name, "shr") == 0 ||
                        strcmp(statement->statement_name, "sar") == 0)
                    {
                        if (op2_op.value->variant != VALUE_CONST && op2_op.value->regalloc != REG_RCX)
                        {
                            shift_mov_performed = 1;
                            zy_emit_2(code, INST_MOV, zy_reg(REG_RCX, type_size(op2_op.value->type)), op2);
                        }
                    }
                    
                    if (!encops_equal(op0, op1))
                    {
                        if (statement->output->type.variant == TYPE_F64 || statement->output->type.variant == TYPE_F32)
                            zy_emit_2(code, INST_MOVAPS, op0, op1);
                        else
                            zy_emit_2(code, INST_MOV, op0, op1);
                    }
                    
                    if (strcmp(statement->statement_name, "add") == 0)
                        zy_emit_2(code, INST_ADD, op0, op2);
                    else if (strcmp(statement->statement_name, "sub") == 0)
                        zy_emit_2(code, INST_SUB, op0, op2);
                    else if (strcmp(statement->statement_name, "mul") == 0)
                        zy_emit_2(code, INST_IMUL, op0, op2);
                    else if (strcmp(statement->statement_name, "imul") == 0)
                        zy_emit_2(code, INST_IMUL, op0, op2);
                    else if (strcmp(statement->statement_name, "shl") == 0)
                    {
                        if (shift_mov_performed)
                            zy_emit_2(code, INST_SHL, op0, zy_reg(REG_RCX, 1));
                        else
                            zy_emit_2(code, INST_SHL, op0, op2);
                    }
                    else if (strcmp(statement->statement_name, "shr") == 0)
                    {
                        if (shift_mov_performed)
                            zy_emit_2(code, INST_SHR, op0, zy_reg(REG_RCX, 1));
                        else
                            zy_emit_2(code, INST_SHR, op0, op2);
                    }
                    else if (strcmp(statement->statement_name, "sar") == 0)
                    {
                        if (shift_mov_performed)
                            zy_emit_2(code, INST_SAR, op0, zy_reg(REG_RCX, 1));
                        else
                            zy_emit_2(code, INST_SAR, op0, op2);
                    }
                    else if (strcmp(statement->statement_name, "and") == 0)
                        zy_emit_2(code, INST_AND, op0, op2);
                    else if (strcmp(statement->statement_name, "or") == 0)
                        zy_emit_2(code, INST_OR, op0, op2);
                    else if (strcmp(statement->statement_name, "xor") == 0)
                        zy_emit_2(code, INST_XOR, op0, op2);
                    else if (strcmp(statement->statement_name, "fadd") == 0 && statement->output->type.variant == TYPE_F32)
                        zy_emit_2(code, INST_ADDSS, op0, op2);
                    else if (strcmp(statement->statement_name, "fadd") == 0 && statement->output->type.variant == TYPE_F64)
                        zy_emit_2(code, INST_ADDSD, op0, op2);
                    else if (strcmp(statement->statement_name, "fsub") == 0 && statement->output->type.variant == TYPE_F32)
                        zy_emit_2(code, INST_SUBSS, op0, op2);
                    else if (strcmp(statement->statement_name, "fsub") == 0 && statement->output->type.variant == TYPE_F64)
                        zy_emit_2(code, INST_SUBSD, op0, op2);
                    else if (strcmp(statement->statement_name, "fmul") == 0 && statement->output->type.variant == TYPE_F32)
                        zy_emit_2(code, INST_MULSS, op0, op2);
                    else if (strcmp(statement->statement_name, "fmul") == 0 && statement->output->type.variant == TYPE_F64)
                        zy_emit_2(code, INST_MULSD, op0, op2);
                    else if (strcmp(statement->statement_name, "fdiv") == 0 && statement->output->type.variant == TYPE_F32)
                        zy_emit_2(code, INST_DIVSS, op0, op2);
                    else if (strcmp(statement->statement_name, "fdiv") == 0 && statement->output->type.variant == TYPE_F64)
                        zy_emit_2(code, INST_DIVSD, op0, op2);
                    else if (strcmp(statement->statement_name, "fxor") == 0)
                        zy_emit_2(code, INST_XORPS, op0, op2);
                    else
                        assert(((void)"TODO", 0));
                }
                else if (strcmp(statement->statement_name, "mov") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    if (op1_op.value->variant == VALUE_STACKADDR)
                    {
                        op1 = zy_mem_change_size(op1, 8);
                        zy_emit_2(code, INST_LEA, op0, op1);
                    }
                    else
                    {
                        if (statement->output->regalloc >= REG_XMM0 && op1_op.value->variant == VALUE_CONST)
                        {
                            const char * name = add_static_i64_anonymous(program, op1_op.value->constant);
                            
                            EncOperand op_dummy = zy_mem(REG_RIP, 0x7FFFFFFF, 8);
                            
                            zy_emit_2(code, INST_MOVSD, op0, op_dummy);
                            add_static_relocation(code->len - 4, name, 4);
                        }
                        else
                        {
                            if (statement->output->type.variant == TYPE_F64 || op1_op.value->type.variant == TYPE_F64 ||
                                statement->output->type.variant == TYPE_F32 || op1_op.value->type.variant == TYPE_F32)
                            {
                                if (value_is_basic_zero_constant(op1_op.value))
                                    zy_emit_2(code, INST_XORPS, op0, op0);
                                else
                                {
                                    if (!op1_op.value->regalloced || !statement->output->regalloced || statement->output->regalloc != op1_op.value->regalloc)
                                        zy_emit_2(code, INST_MOVAPS, op0, op1);
                                }
                            }
                            else
                            {
                                if (!op1_op.value->regalloced || !statement->output->regalloced || statement->output->regalloc != op1_op.value->regalloc)
                                    zy_emit_2(code, INST_MOV, op0, op1);
                            }
                        }
                    }
                }
                else if (strcmp(statement->statement_name, "store") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    EncOperand op1 = get_basic_encoperand_mem(op1_op.value, 1);
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    
                    assert(((void)"TODO", type_size(op2_op.value->type) <= 8));
                    
                    if (op2_op.value->variant == VALUE_CONST &&
                        type_size(op2_op.value->type) == 8 &&
                        (((int64_t)op2_op.value->constant) >  (int64_t)0x7FFFFFFF ||
                         ((int64_t)op2_op.value->constant) < -(int64_t)0x80000000))
                    {
                        uint64_t n = op2_op.value->constant;
                        Value * lo = make_const_value(TYPE_I32, n & 0xFFFFFFFF);
                        Value * hi = make_const_value(TYPE_I32, n >> 32);
                        EncOperand op_lo = get_basic_encoperand(lo);
                        EncOperand op_hi = get_basic_encoperand(hi);
                        
                        EncOperand addr_lower = zy_mem_change_size(op1, 4);
                        EncOperand addr_higher = zy_mem_add_offset(addr_lower, 4);
                        zy_emit_2(code, INST_MOV, addr_lower, op_lo);
                        zy_emit_2(code, INST_MOV, addr_higher, op_hi);
                    }
                    else
                    {
                        if (op2_op.value->type.variant == TYPE_F64)
                            zy_emit_2(code, INST_MOVQ, op1, op2);
                        else if (op2_op.value->type.variant == TYPE_F32)
                            zy_emit_2(code, INST_MOVD, op1, op2);
                        else
                            zy_emit_2(code, INST_MOV, op1, op2);
                    }
                }
                else if (strcmp(statement->statement_name, "load") == 0)
                {
                    Operand type_op = statement->args[0];
                    assert(type_op.variant == OP_KIND_TYPE);
                    Operand op1_op = statement->args[1];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    EncOperand op1 = get_basic_encoperand_mem(op1_op.value, 1);
                    
                    if (statement->output->type.variant == TYPE_F64)
                        zy_emit_2(code, INST_MOVQ, op0, op1);
                    else if (statement->output->type.variant == TYPE_F32)
                        zy_emit_2(code, INST_MOVD, op0, op1);
                    else
                        zy_emit_2(code, INST_MOV, op0, op1);
                }
                else if (strcmp(statement->statement_name, "goto") == 0)
                {
                    Operand target_op = statement->args[0];
                    assert(target_op.variant == OP_KIND_TEXT);
                    
                    Value * dummy = make_const_value(TYPE_I32, 0x7FFFFFFF);
                    EncOperand op_dummy = get_basic_encoperand(dummy);
                    
                    Block * target_block = find_block(func, target_op.text);
                    size_t ba_len = array_len(target_block->args, Value *);
                    size_t sa_len = array_len(statement->args, Operand) - 1;
                    assert(((void)"wrong number of arguments to block", ba_len == sa_len));
                    
                    if (reg_shuffle_needed(target_block->args, statement->args + 1, ba_len))
                        reg_shuffle_block_args(code, target_block->args, statement->args + 1, ba_len);
                    
                    if (strcmp(target_op.text, next_block->name) != 0)
                    {
                        zy_emit_1(code, INST_JMP, op_dummy);
                        add_label_relocation(code->len - 4, target_op.text, 4);
                    }
                }
                else if (strcmp(statement->statement_name, "if") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    
                    Operand target_op = statement->args[1];
                    assert(target_op.variant == OP_KIND_TEXT);
                    
                    Operand target_op2;
                    memset(&target_op2, 0, sizeof(Operand));
                    size_t separator_pos = find_separator_index(statement->args);
                    assert(separator_pos);
                    assert(array_len(statement->args, Operand) > separator_pos + 1); // second label must exist
                    target_op2 = statement->args[separator_pos + 1];
                    assert(target_op2.variant == OP_KIND_TEXT);
                    
                    int jcc_yin  = INST_JZ;
                    int jcc_yang = INST_JNZ;
                    
                    if (prev_statement && str_begins_with(prev_statement->statement_name, "cmp_"))
                    {
                        if (strcmp(prev_statement->statement_name, "cmp_g") == 0)
                        {
                            jcc_yin = INST_JBE;
                            jcc_yang = INST_JNBE;
                        }
                        else
                        {
                            assert(((void)"TODO", 0));
                        }
                    }
                    else
                    {
                        zy_emit_2(code, INST_TEST, op1, op1);
                    }
                    
                    Value * dummy = make_const_value(TYPE_I32, 0x7FFFFFFF);
                    EncOperand op_dummy = get_basic_encoperand(dummy);
                    
                    Operand * if_s_args = statement->args + 2;
                    Block * if_target_block = find_block(func, target_op.text);
                    size_t iba_len = array_len(if_target_block->args, Value *);
                    size_t isa_len = separator_pos - 2;
                    assert(((void)"wrong number of arguments to block", iba_len == isa_len));
                    
                    Operand * else_s_args = statement->args + separator_pos + 2;
                    Block * else_target_block = find_block(func, target_op2.text);
                    size_t eba_len = array_len(else_target_block->args, Value *);
                    size_t esa_len = array_len(statement->args, Operand) - separator_pos - 2;
                    assert(((void)"wrong number of arguments to block", eba_len == esa_len));
                    
                    uint8_t if_shuffle_needed = reg_shuffle_needed(if_target_block->args, if_s_args, iba_len);
                    uint8_t else_shuffle_needed = reg_shuffle_needed(else_target_block->args, else_s_args, eba_len);
                    
                    //printf("00-`-`-`1 - -3`2    %d %d\n", if_shuffle_needed, else_shuffle_needed);
                    
                    if (if_shuffle_needed && else_shuffle_needed)
                    {
                        zy_emit_1(code, jcc_yin, op_dummy);
                        size_t jump_over_loc = code->len;
                        
                        reg_shuffle_block_args(code, if_target_block->args, if_s_args, iba_len);
                        
                        zy_emit_1(code, INST_JMP, op_dummy);
                        add_label_relocation(code->len - 4, target_op.text, 4);
                        
                        size_t jump_over_target = code->len;
                        int32_t jump_over_len = jump_over_target - jump_over_loc;
                        memcpy(code->data + jump_over_loc - 4, &jump_over_len, 4);
                        
                        reg_shuffle_block_args(code, else_target_block->args, else_s_args, eba_len);
                        
                        if (strcmp(target_op2.text, next_block->name) != 0)
                        {
                            zy_emit_1(code, INST_JMP, op_dummy);
                            add_label_relocation(code->len - 4, target_op2.text, 4);
                        }
                    }
                    else if (else_shuffle_needed)
                    {
                        zy_emit_1(code, jcc_yang, op_dummy);
                        add_label_relocation(code->len - 4, target_op.text, 4);
                        
                        reg_shuffle_block_args(code, else_target_block->args, else_s_args, eba_len);
                        
                        if (strcmp(target_op2.text, next_block->name) != 0)
                        {
                            zy_emit_1(code, INST_JMP, op_dummy);
                            add_label_relocation(code->len - 4, target_op2.text, 4);
                        }
                    }
                    else if (if_shuffle_needed)
                    {
                        zy_emit_1(code, jcc_yin, op_dummy);
                        add_label_relocation(code->len - 4, target_op2.text, 4);
                        
                        reg_shuffle_block_args(code, if_target_block->args, if_s_args, iba_len);
                        
                        if (strcmp(target_op.text, next_block->name) != 0)
                        {
                            zy_emit_1(code, INST_JMP, op_dummy);
                            add_label_relocation(code->len - 4, target_op.text, 4);
                        }
                    }
                    else if (strcmp(target_op2.text, next_block->name) == 0)
                    {
                        zy_emit_1(code, jcc_yang, op_dummy);
                        add_label_relocation(code->len - 4, target_op.text, 4);
                    }
                    else if (strcmp(target_op.text, next_block->name) == 0)
                    {
                        zy_emit_1(code, jcc_yin, op_dummy);
                        add_label_relocation(code->len - 4, target_op2.text, 4);
                    }
                    else
                    {
                        zy_emit_1(code, jcc_yin, op_dummy);
                        add_label_relocation(code->len - 4, target_op2.text, 4);
                        zy_emit_1(code, INST_JMP, op_dummy);
                        add_label_relocation(code->len - 4, target_op.text, 4);
                    }
                }
                else if (str_begins_with(statement->statement_name, "cmp_"))
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    zy_emit_2(code, INST_CMP, op1, op2);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    
                    if (!next_statement || strcmp(next_statement->statement_name, "if") != 0)
                    {
                        if ((strcmp(statement->statement_name, "cmp_g") == 0))
                            zy_emit_1(code, INST_SETNBE, op0);
                        else
                            assert(((void)"TODO", 0));
                    }
                }
                else if (strcmp(statement->statement_name, "uint_to_float") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_TYPE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    // FIXME: handle sizes other than i32 properly
                    // i8/i16 need zero extension
                    // i64 needs overflow handling (CVTSI2SD/CVTSI2SS are signed)
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    
                    if (op1_op.rawtype.variant == TYPE_F64)
                        zy_emit_2(code, INST_CVTSI2SD, op0, op2);
                    else if (op1_op.rawtype.variant == TYPE_F32)
                        zy_emit_2(code, INST_CVTSI2SS, op0, op2);
                    else
                        assert(((void)"TODO", 0));
                }
                else if (strcmp(statement->statement_name, "bitcast") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_TYPE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    assert(type_size(op1_op.rawtype) == type_size(op2_op.value->type));
                    
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    
                    if (type_is_intreg(statement->output->type) && type_is_intreg(op2_op.value->type))
                        zy_emit_2(code, INST_MOV, op0, op2);
                    else if (!type_is_intreg(statement->output->type) && !type_is_intreg(op2_op.value->type))
                        zy_emit_2(code, INST_MOVAPS, op0, op2);
                    else
                        zy_emit_2(code, INST_MOVQ, op0, op2);
                }
                else if (strcmp(statement->statement_name, "symbol_lookup_unsized") == 0 ||
                         strcmp(statement->statement_name, "symbol_lookup") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_TEXT);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    
                    const char * symbol = op1_op.text;
                    
                    EncOperand op_dummy = zy_mem(REG_RIP, 0x7FFFFFFF, 8);
                    
                    zy_emit_2(code, INST_LEA, op0, op_dummy);
                    add_symbol_relocation(code->len - 4, symbol, 4);
                }
                else if (strcmp(statement->statement_name, "call_eval") == 0 ||
                         strcmp(statement->statement_name, "call") == 0)
                {
                    Operand op_target = statement->args[0];
                    assert(op_target.variant == OP_KIND_VALUE);
                    
                    assert(statement->output);
                    assert(statement->output->regalloced);
                    
                    EncOperand target = get_basic_encoperand(op_target.value);
                    EncOperand op0 = get_basic_encoperand(statement->output);
                    
                    reg_shuffle_call(code, statement);
                    
                    zy_emit_1(code, INST_CALL, target);
                    
                    Value * value = statement->output;
                    if (type_is_intreg(value->type))
                        zy_emit_2(code, INST_MOV, op0, zy_reg(REG_RAX, type_size(value->type)));
                    else
                        zy_emit_2(code, INST_MOVQ, op0, zy_reg(REG_XMM0, 8));
                    
                    func->performs_calls = 1;
                }
                else if (strcmp(statement->statement_name, "breakpoint") == 0)
                {
                    zy_emit_0(code, INST_INT3);
                }
                else
                {
                    printf("culprit: %s\n", statement->statement_name);
                    assert(((void)"unhandled operation!", 0));
                }
            }
        }
        apply_label_relocations(program, code, func);
    }
    
    apply_static_relocations(program, code, program);
    apply_symbol_relocations(program, code, *symbollist);
    
    return code;
}

#endif // BBAE_EMISSION
