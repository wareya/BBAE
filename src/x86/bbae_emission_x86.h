#ifndef BBAE_EMISSION
#define BBAE_EMISSION

#include "regalloc_x86.h"
#include "emitter_x86.h"
#include "../compiler_common.h"

typedef struct _NameUsageInfo
{
    uint64_t loc;
    const char * name;
    uint8_t size;
    // TODO: whether it's end-relative or start-relative (currently only end)
} NameUsageInfo;

static void add_relocation(NameUsageInfo ** usages, uint64_t loc, const char * name, uint8_t size)
{
    NameUsageInfo info = {loc, name, size};
    if (!*usages)
        *usages = (NameUsageInfo *)zero_alloc(0);
    
    array_push((*usages), NameUsageInfo, info);
}
static void apply_relocations(NameUsageInfo ** usages, byte_buffer * code, int64_t (*target_lookup_func)(const char *, void *), void * userdata)
{
    assert(*usages);
    for (size_t u = 0; u < array_len((*usages), NameUsageInfo); u++)
    {
        NameUsageInfo info = (*usages)[u];
        //Block * block = find_block(func, info.name);
        //if (!block)
        //{
        //    printf("failed to find block with name %s\n", info.name);
        //    assert(0); 
        //}
        int64_t target = target_lookup_func(info.name, userdata);
        int64_t rewrite_loc = info.loc;
        if (info.size == 4)
        {
            int64_t diff = rewrite_loc + 4 - target;
            assert(diff >= -2147483648 && diff <= 2147483647);
            assert(rewrite_loc + 4 < (int64_t)code->len);
            memcpy(code->data + rewrite_loc, &diff, 4);
        }
        else if (info.size == 1)
        {
            int64_t diff = rewrite_loc + 1 - target;
            assert(diff >= -128 && diff <= 127);
            assert(rewrite_loc + 1 < (int64_t)code->len);
            memcpy(code->data + rewrite_loc, &diff, 1);
        }
        else
            assert(("TODO", 0));
    }
}

static NameUsageInfo * _emitter_label_usages = 0;
static void add_label_relocation(uint64_t loc, const char * name, uint8_t size)
{
    add_relocation(&_emitter_label_usages, loc, name, size);
}
static int64_t get_block_loc_or_assert(const char * name, void * _func)
{
    Function * func = (Function *)_func;
    Block * block = find_block(func, name);
    if (!block)
    {
        printf("failed to find block with name %s\n", name);
        assert(0); 
    }
    return block->start_offset;
}
static void apply_label_relocations(byte_buffer * code, Function * func)
{
    apply_relocations(&_emitter_label_usages, code, get_block_loc_or_assert, (void *)func);
}

static NameUsageInfo * static_addr_relocations = 0;
static void add_static_relocation(uint64_t loc, const char * name, uint8_t size)
{
    add_relocation(&static_addr_relocations, loc, name, size);
}

static int64_t get_static_or_assert(const char * name, void * _program)
{
    Program * program = (Program *)_program;
    StaticData stat = find_static(program, name);
    if (!stat.name)
    {
        printf("failed to find static with name %s\n", name);
        assert(0); 
    }
    return stat.location;
}
static void apply_static_relocations(byte_buffer * code, Program * program)
{
    // allocate and align statics
    for (size_t i = 0; i < array_len(program->statics, StaticData); i++)
    {
        StaticData stat = program->statics[i];
        while (code->len % size_guess_align(type_size(stat.type)))
            byte_push(code, 0);
        
        stat.location = code->len;
        
        if (type_size(stat.type) <= 8)
            bytes_push(code, (uint8_t *)&stat.init_data_short, type_size(stat.type));
        else
            bytes_push(code, stat.init_data_long, type_size(stat.type));
    }
    // apply relocations pointing at statics
    apply_relocations(&static_addr_relocations, code, get_static_or_assert, (void *)program);
}

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

static EncOperand get_basic_encoperand(Value * value)
{
    assert(value->variant == VALUE_CONST || value->variant == VALUE_STACKADDR || value->regalloced);
    if (value->variant == VALUE_CONST)
    {
        assert(type_is_valid(value->type));
        assert(("TODO", type_size(value->type) <= 8));
        return zy_imm(value->constant, type_size(value->type));
    }
    else if (value->variant == VALUE_SSA || value->variant == VALUE_ARG)
    {
        assert(type_is_valid(value->type));
        return zy_reg(value->regalloc, type_size(value->type));
    }
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
        
        if (func->stack_height)
        {
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
            EncOperand height = zy_imm(func->stack_height, 4);
            
            zy_emit_1(code, INST_PUSH, rbp);
            zy_emit_2(code, INST_MOV, rbp, rsp);
            zy_emit_2(code, INST_SUB, rsp, height);
            
            for (size_t i = 0; i < sizeof(func->written_registers); i++)
            {
                size_t n = 0;
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
            
            block->start_offset = code->len;
            
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                
                if (strcmp(statement->statement_name, "return") == 0)
                {
                    Operand op = statement->args[0];
                    assert(op.variant == OP_KIND_VALUE);
                    
                    EncOperand op2 = get_basic_encoperand(op.value);
                    if (op.value->type.variant == TYPE_F64)
                    {
                        EncOperand op1 = zy_reg(REG_XMM0, type_size(op.value->type));
                        if (!encops_equal(op1, op2))
                            zy_emit_2(code, INST_MOVQ, op1, op2);
                    }
                    else if (op.value->type.variant == TYPE_F64)
                    {
                        EncOperand op1 = zy_reg(REG_XMM0, type_size(op.value->type));
                        if (!encops_equal(op1, op2))
                            zy_emit_2(code, INST_MOVD, op1, op2);
                    }
                    else
                    {
                        EncOperand op1 = zy_reg(REG_RAX, type_size(op.value->type));
                        if (!encops_equal(op1, op2))
                            zy_emit_2(code, INST_MOV, op1, op2);
                    }
                    
                    for (size_t i = 0; i < sizeof(func->written_registers); i++)
                    {
                        size_t n = 0;
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
                else if (strcmp(statement->statement_name, "add") == 0 ||
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
                        EncOperand temp = op1;
                        op1 = op2;
                        op2 = temp;
                    }
                    if (!encops_equal(op0, op1))
                        zy_emit_2(code, INST_MOV, op0, op1);
                    if (strcmp(statement->statement_name, "add") == 0)
                        zy_emit_2(code, INST_ADD, op0, op2);
                    else if (strcmp(statement->statement_name, "fmul") == 0 && statement->output->type.variant == TYPE_F32)
                        zy_emit_2(code, INST_MULSS, op0, op2);
                    else if (strcmp(statement->statement_name, "fmul") == 0 && statement->output->type.variant == TYPE_F64)
                        zy_emit_2(code, INST_MULSD, op0, op2);
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
                        op1.mem.size = 8;
                        zy_emit_2(code, INST_LEA, op0, op1);
                    }
                    else
                    {
                        if (statement->output->regalloc >= REG_XMM0 && op1_op.value->variant == VALUE_CONST)
                        {
                            const char * name = make_temp_name();
                            add_static_i64(program, name, op1_op.value->constant);
                            
                            EncOperand op_dummy = zy_mem(REG_RIP, 0x7FFFFFFF, 8);
                            
                            if (statement->output->type.variant == TYPE_F64)
                                zy_emit_2(code, INST_MOVQ, op0, op_dummy);
                            else
                                zy_emit_2(code, INST_MOVD, op0, op_dummy);
                            add_static_relocation(code->len - 4, name, 4);
                        }
                        else
                        {
                            if (statement->output->type.variant == TYPE_F64 || op1_op.value->type.variant == TYPE_F64)
                                zy_emit_2(code, INST_MOVQ, op0, op1);
                            else if (statement->output->type.variant == TYPE_F32 || op1_op.value->type.variant == TYPE_F32)
                                zy_emit_2(code, INST_MOVD, op0, op1);
                            else
                                zy_emit_2(code, INST_MOV, op0, op1);
                        }
                    }
                }
                else if (strcmp(statement->statement_name, "store") == 0)
                {
                    Operand op1_op = statement->args[0];
                    assert(op1_op.variant == OP_KIND_VALUE);
                    Operand op2_op = statement->args[1];
                    assert(op2_op.variant == OP_KIND_VALUE);
                    
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    EncOperand op2 = get_basic_encoperand(op2_op.value);
                    
                    assert(("TODO", type_size(op2_op.value->type) <= 8));
                    
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
                        EncOperand op2 = get_basic_encoperand(op2_op.value);
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
                    EncOperand op1 = get_basic_encoperand(op1_op.value);
                    
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
                    
                    zy_emit_1(code, INST_JMP, op_dummy);
                    add_label_relocation(code->len - 4, target_op.text, 4);
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
                    for (size_t i = 2; i < array_len(statement->args, Operand); i++)
                    {
                        if (statement->args[i].variant == OP_KIND_SEPARATOR)
                        {
                            assert(array_len(statement->args, Operand) > i + 1);
                            target_op2 = statement->args[i + 1];
                            break;
                        }
                    }
                    assert(target_op2.variant == OP_KIND_TEXT);
                    
                    printf("testing %zu against itself...\n", op1_op.value->regalloc);
                    size_t b = code->len;
                    printf("b %zu\n", b);
                    zy_emit_2(code, INST_TEST, op1, op1);
                    size_t a = code->len;
                    printf("a %zu\n", a);
                    for (size_t i = b; i < a; i++)
                        printf("  %02X\n", code->data[i]);
                    
                    Value * dummy_short = make_const_value(TYPE_I8, 0);
                    EncOperand op_dummy_short = get_basic_encoperand(dummy_short);
                    
                    zy_emit_1(code, INST_JZ, op_dummy_short);
                    add_label_relocation(code->len - 1, target_op.text, 1);
                    
                    Value * dummy = make_const_value(TYPE_I32, 0x7FFFFFFF);
                    EncOperand op_dummy = get_basic_encoperand(dummy);
                    zy_emit_1(code, INST_JMP, op_dummy);
                    add_label_relocation(code->len - 4, target_op2.text, 4);
                    
                    // TODO/FIXME: handle register shuffling!!!!
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
                    
                    if ((strcmp(statement->statement_name, "cmp_g") == 0))
                        zy_emit_1(code, INST_SETNB, op0);
                    else
                        assert(("TODO", 0));
                    
                    // TODO/FIXME: handle register shuffling!!!!
                }
                else
                {
                    printf("culprit: %s\n", statement->statement_name);
                    assert(("unhandled operation!", 0));
                }
            }
        }
        apply_label_relocations(code, func);
    }
    
    apply_static_relocations(code, program);
    
    return code;
}

#endif // BBAE_EMISSION
