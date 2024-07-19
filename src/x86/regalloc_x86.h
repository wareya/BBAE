#ifndef BBAE_REGALLOC
#define BBAE_REGALLOC

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "abi_x86.h"
#include "../memory.h"
#include "../compiler_common.h"

#define BBAE_REGISTER_CAPACITY (16)
#ifdef BBAE_DEBUG_SPILLS
#define BBAE_REGISTER_COUNT (3)
#else
#define BBAE_REGISTER_COUNT (16)
#endif

static int64_t first_empty(Value ** array, size_t len, uint64_t allow_mask)
{
    // TODO: if block has no calls in it, prefer non-callee-saved registers
    int64_t found = -1;
    for (size_t n = 0; n < len; n++)
    {
        if (!((allow_mask >> n) & 1))
            continue;
        if (!array[n])
        {
            found = n;
            break;
        }
    }
    return found;
}

// true if the type can be stored in an integer register
static uint8_t type_is_intreg(Type type)
{
    if (type_size(type) > 8)
        return 0;
    if (type_is_agg(type))
        return !type_is_float_only_agg(type);
    return type_is_int(type) || type_is_ptr(type);
}

static uint8_t value_reg_is_thrashed(Value ** reg_int_alloced, Value ** reg_float_alloced, Value * value)
{
    if (value->regalloced && !value->spilled)
    {
        assert(value->regalloc < 32);
        if (value->regalloc < _ABI_XMM0 && !reg_int_alloced[value->regalloc])
            return 0;
        else if (value->regalloc >= _ABI_XMM0 && !reg_float_alloced[value->regalloc - _ABI_XMM0])
            return 0;
    }
    return 1;
}

typedef struct _RegAllocRules
{
    // for up to 8 operands (0 is output, 1 is input 0, 2 is input 1, etc, up to 1 output and 7 inputs):
    // which registers from 0 to 63 are allowed
    // for calls and branches, all input registers are always allowed, and output reg is determined by the arch
    uint64_t allowed_output_registers;
    uint64_t clobbered_registers;
    
    // SPECIAL CASE: for any of the above properties to have any effect, this MUST be set to 1.
    // if this is set to 0, then the backend should IGNORE all the above.
    uint8_t is_special;
} RegAllocRules;

RegAllocRules regalloc_rule_determiner(Statement * statement)
{
    RegAllocRules ret;
    memset(&ret, 0, sizeof(RegAllocRules));
    
    ret.allowed_output_registers = 0xFFFFFFFF;
    
    if (strcmp(statement->statement_name, "div") == 0 ||
        strcmp(statement->statement_name, "idiv") == 0)
    {
        ret.is_special = 1;
        ret.allowed_output_registers = (1 << _ABI_RAX);
        ret.clobbered_registers |= (1 << _ABI_RAX);
        // x86 8-bit div/idiv puts remainder in AH (upper 8 bits of 16-bit AX) instead of DL (lower 8 bits of 16-bit DX)
        if (statement->output->type.variant != TYPE_I8)
            ret.clobbered_registers |= (1 << _ABI_RDX);
    }
    else if (strcmp(statement->statement_name, "rem") == 0 ||
             strcmp(statement->statement_name, "irem") == 0)
    {
        ret.is_special = 1;
        // x86 8-bit div/idiv puts remainder in AH (upper 8 bits of 16-bit AX) instead of DL (lower 8 bits of 16-bit DX)
        if (statement->output->type.variant != TYPE_I8)
        {
            ret.allowed_output_registers = (1 << _ABI_RDX);
            ret.clobbered_registers |= (1 << _ABI_RAX);
            ret.clobbered_registers |= (1 << _ABI_RDX);
        }
        else
        {
            ret.allowed_output_registers = (1 << _ABI_RAX);
            ret.clobbered_registers |= (1 << _ABI_RAX);
        }
    }
    else if (strcmp(statement->statement_name, "shl") == 0 ||
             strcmp(statement->statement_name, "shr") == 0 ||
             strcmp(statement->statement_name, "sar") == 0 ||
             strcmp(statement->statement_name, "shr_unsafe") == 0 ||
             strcmp(statement->statement_name, "sar_unsafe") == 0)
    {
        if (statement->args[0].value->variant != VALUE_CONST)
        {
            ret.is_special = 1;
            ret.clobbered_registers |= (1 << _ABI_RCX);
        }
    }
    else if (strcmp(statement->statement_name, "call") == 0 ||
             strcmp(statement->statement_name, "call_eval") == 0)
    {
        ret.is_special = 1;
        if (type_is_int(statement->output->type))
            ret.allowed_output_registers = (1 << _ABI_RAX);
        else if (type_is_float(statement->output->type))
            ret.allowed_output_registers = (1 << _ABI_XMM0);
        else
            assert(((void)"TODO", 0));
        // call clobbers are determined by ABI and handled by machine code emitter, not regalloc
    }
    
    return ret;
}

static void do_spill(Function * func, Block * block, Value ** reg_int_alloced, Value ** reg_float_alloced, Value * spillee, int64_t to_spill_reg, uint64_t to_spill_num, uint64_t allowed_mask, size_t * i)
{
    printf("want to spill reg %zu (%p)\n", to_spill_reg, (void *)spillee);
    
    if (to_spill_reg >= _ABI_XMM0)
    {
        assert(reg_float_alloced[to_spill_reg - _ABI_XMM0] == spillee);
        reg_float_alloced[to_spill_reg - _ABI_XMM0] = 0;
    }
    else
    {
        assert(reg_int_alloced[to_spill_reg] == spillee);
        reg_int_alloced[to_spill_reg] = 0;
    }
    
    assert(spillee->regalloced);
    assert(spillee->regalloc >= 0);
    assert(!spillee->spilled);
    
    // first use is after current statement
    int64_t temp = -1;
    if (spillee->regalloc <= _ABI_R15)
        temp = first_empty(reg_int_alloced, BBAE_REGISTER_COUNT, allowed_mask);
    else
        temp = first_empty(reg_float_alloced, BBAE_REGISTER_COUNT, allowed_mask >> 16) + _ABI_XMM0;
    
    if (temp >= 0)
    {
        if (array_len(spillee->edges_out, Statement *) > 0 && spillee->edges_out[0]->num > block->statements[*i]->num)
        {
            // FIXME
            //if (!spillee->arg)
            {
                spillee->regalloc = temp;
                return;
            }
            //else
            //    printf("can't fast-spill %s because it's an arg\n", spillee->arg);
        }
        else
        {
            printf("can't fast-spill %s because it has earlier users\n", spillee->arg ? spillee->arg : spillee->ssa->output_name);
            // FIXME: support MOV-spilling. needs to rewrite other descendants.
            Statement * spill = new_statement();
            spill->output_name = make_temp_name();
            spill->statement_name = strcpy_z("mov");
            array_push(spill->args, Operand, new_op_val(spillee));
            spill->output = make_value(spillee->type);
            spill->output->variant = VALUE_SSA;
            spill->output->regalloced = 1;
            spill->output->regalloc = temp;
            spill->output->ssa = spill;
            
            if (temp >= _ABI_XMM0)
                reg_float_alloced[temp - _ABI_XMM0] = spill->output;
            else 
                reg_int_alloced[temp] = spill->output;
            
            array_insert(block->statements, Statement *, *i, spill);
            *i += 1;
            
            for (size_t j = 0; j < array_len(spillee->edges_out, Statement *); j++)
            {
                Statement * st = spillee->edges_out[j];
                if (st->num >= to_spill_num)
                {
                    Operand temp_op = new_op_val(spillee);
                    ptrdiff_t in_arg_index = array_find(st->args, Operand, temp_op);
                    assert(in_arg_index >= 0);
                    st->args[in_arg_index] = new_op_val(spill->output);
                    
                    array_push(spill->output->edges_out, Statement *, st);
                    array_erase(spillee->edges_out, Statement *, j);
                    j -= 1;
                }
            }
            return;
        }
    }
    else
        printf("can't fast-spill %s because a suitable register was not found. allowed mask: %08zX\n", spillee->arg ? spillee->arg : spillee->ssa->output_name, allowed_mask);
    
    // spill statements don't need to be numbered because they're never an outward edge of an SSA value
    Value * spill_slot = add_stack_slot(func, make_temp_name(), type_size(spillee->type));
    spillee->spilled = spill_slot->slotinfo;
    
    Statement * spill = new_statement();
    spill->statement_name = strcpy_z("store");
    array_push(spill->args, Operand, new_op_val(spill_slot));
    array_push(spill->args, Operand, new_op_val(spillee));
    
    array_insert(block->statements, Statement *, *i, spill);
    *i += 1;
    
    // insert a load and rewrite usages of the spilled variable
    uint8_t first = 1;
    Statement * unspill = 0;
    for (size_t j = 0; j < array_len(spillee->edges_out, Statement *); j++)
    {
        Statement * st = spillee->edges_out[j];
        printf("checking for unspilling %s... %zu vs %zu\n", spillee->arg ? spillee->arg : spillee->ssa->output_name, st->num, to_spill_num);
        if (st->num >= to_spill_num)
        {
            if (first)
            {
                printf("unspilling...\n");
                
                ptrdiff_t inst_index = ptr_array_find(block->statements, st);
                assert(inst_index != -1);
                
                unspill = new_statement();
                unspill->output_name = make_temp_name();
                unspill->statement_name = strcpy_z("load");
                unspill->output = make_value(spillee->type);
                unspill->output->variant = VALUE_SSA;
                unspill->output->ssa = unspill;
                
                Operand op1 = new_op_type(spillee->type);
                array_push(unspill->args, Operand, op1);
                Operand op2 = new_op_val(spill_slot);
                array_push(unspill->args, Operand, op2);
                connect_statement_to_operand(unspill, op2);
                
                array_insert(block->statements, Statement *, inst_index, unspill);
            }
            first = 0;
            
            assert(unspill);
            assert(unspill->output);
            assert(unspill->output->edges_out);
            
            // there is an args entry for each edges_in entry, even if two instructions are connected twice
            // e.g. a = load ...; c = add a a
            // so we don't need to loop here
            
            Operand temp_op = new_op_val(spillee);
            ptrdiff_t in_arg_index = array_find(st->args, Operand, temp_op);
            assert(in_arg_index > -1);
            st->args[in_arg_index] = new_op_val(unspill->output);
            
            array_push(unspill->output->edges_out, Statement *, st);
            array_erase(spillee->edges_out, Statement *, j);
            j -= 1;
        }
    }
}

// picks the allocated register whose next use is the furthest into the future and not in the current statement
static void find_spillable(Statement * statement, uint8_t is_int, Value ** reg_int_alloced, Value ** reg_float_alloced,
                           int64_t * to_spill_reg, uint64_t * to_spill_num, uint64_t allowed_mask)
{
    for (size_t n = 0; n < BBAE_REGISTER_COUNT; n++)
    {
        if (!((allowed_mask >> n) & 1))
            continue;
        Value * spill_candidate = 0;
        if (is_int)
        {
            if (!(reg_int_alloced[n] && reg_int_alloced[n] != (Value *)-1 && n != _ABI_RSP && n != _ABI_RBP))
                continue;
            spill_candidate = reg_int_alloced[n];
        }
        else
        {
            if (!(reg_float_alloced[n] && reg_float_alloced[n] != (Value *)-1))
                continue;
            spill_candidate = reg_float_alloced[n];
        }
        
        // FIXME: ????
        if (ptr_array_find(statement->args, spill_candidate) != (size_t)-1)
            continue;
        
        uint64_t candidate_last_num = 0;
        for (size_t i = 0; i < array_len(spill_candidate->edges_out, Value *); i++)
        {
            uint64_t temp = spill_candidate->edges_out[i]->num;
            if (temp > statement->num)
            {
                candidate_last_num = temp;
                break;
            }
        }
        
        if (*to_spill_reg == -1 || candidate_last_num > *to_spill_num)
        {
            *to_spill_reg = n;
            *to_spill_num = candidate_last_num;
        }
    }
}

static void regalloc_func_args(Function * func, Value ** reg_int_alloced, Value ** reg_float_alloced)
{
    abi_reset_state();
    for (size_t i = 0; i < array_len(func->args, Value *); i++)
    {
        Value * value = func->args[i];
        if (array_len(value->edges_out, Statement *) == 0)
            continue;
        if (type_is_basic(value->type))
        {
            int64_t where = abi_get_next_arg_basic(type_is_float(value->type));
            assert(((void)"arg reg spill not yet supported", where >= 0));
            
            if (where >= _ABI_XMM0)
                reg_float_alloced[where - _ABI_XMM0] = value;
            else 
                reg_int_alloced[where] = value;
            
            printf("allocated register %zd to func arg %s\n", where, value->arg);
            
            value->regalloc = where;
            value->regalloced = 1;
        }
        else
            assert(((void)"aggregate args not yet supported", 0));
    }
}

static void regalloc_block_args(Block * block, Value ** reg_int_alloced, Value ** reg_float_alloced)
{
    for (size_t i = 0; i < array_len(block->args, Value *); i++)
    {
        // preferentially allocate based on output allocation of entry blocks
        Value * value = block->args[i];
        int64_t where = 0;
        uint8_t where_found = 0;
        
        for (size_t j = 0; j < array_len(block->edges_in, Statement *); j++)
        {
            Statement * entry = block->edges_in[j];
            if (strcmp(entry->statement_name, "goto") == 0)
            {
                Operand op = entry->args[i + 1];
                assert(op.value);
                if (!value_reg_is_thrashed(reg_int_alloced, reg_float_alloced, op.value))
                {
                    where = op.value->regalloc;
                    where_found = 1;
                    break;
                }
            }
            else if (strcmp(entry->statement_name, "if") == 0)
            {
                if (entry->args[1].text == block->name)
                {
                    Operand op = entry->args[i + 2];
                    assert(op.value);
                    if (!value_reg_is_thrashed(reg_int_alloced, reg_float_alloced, op.value))
                    {
                        where = op.value->regalloc;
                        where_found = 1;
                        break;
                    }
                }
                
                size_t separator_pos = find_separator_index(entry->args);
                assert(separator_pos); // blocks must be split at if statements
                
                if (entry->args[separator_pos + 1].text == block->name)
                {
                    assert(i + separator_pos + 2 < array_len(entry->args, Operand));
                    Operand op = entry->args[i + separator_pos + 2];
                    assert(op.value);
                    
                    if (!value_reg_is_thrashed(reg_int_alloced, reg_float_alloced, op.value))
                    {
                        where = op.value->regalloc;
                        where_found = 1;
                        break;
                    }
                }
            }
            else
                assert(0);
        }
        
        if (!where_found)
        {
            if (type_is_intreg(value->type))
                where = first_empty(reg_int_alloced, BBAE_REGISTER_COUNT, 0xFFFF);
            else if (type_is_float(value->type))
                where = first_empty(reg_float_alloced, BBAE_REGISTER_COUNT, 0xFFFF) + _ABI_XMM0;
            else
                assert(((void)"TODO", 0));
        }
        
        if (where < 0)
            assert(((void)"spilled block arguments not yet supported", 0));
        
        if (where >= _ABI_XMM0)
            reg_float_alloced[where - _ABI_XMM0] = value;
        else 
            reg_int_alloced[where] = value;
        
        value->regalloc = where;
        value->regalloced = 1;
    }
}

static void expire_unused_regs(Value ** reg_int_alloced, Value ** reg_float_alloced)
{
#ifndef BBAE_DEBUG_SPILLS
    for (size_t n = 0; n < BBAE_REGISTER_COUNT; n++)
    {
        if (reg_int_alloced[n] && reg_int_alloced[n] != (Value *)-1)
        {
            Value * value = reg_int_alloced[n];
            assert(value->alloced_use_count <= array_len(value->edges_out, Value *));
            if (value->alloced_use_count == array_len(value->edges_out, Value *))
            {
                //printf("freeing int register %zu... %zu vs %zu\n", n, value->alloced_use_count, array_len(value->edges_out, Value *));
                reg_int_alloced[n] = 0;
            }
        }
        if (reg_float_alloced[n] && reg_float_alloced[n] != (Value *)-1)
        {
            Value * value = reg_float_alloced[n];
            assert(value->alloced_use_count <= array_len(value->edges_out, Value *));
            if (value->alloced_use_count == array_len(value->edges_out, Value *))
            {
                //printf("freeing float register %zu...\n", n);
                reg_float_alloced[n] = 0;
            }
        }
    }
#endif // BBAE_DEBUG_SPILLS
}

static void do_regalloc_block(Function * func, Block * block)
{
    Value * reg_int_alloced[BBAE_REGISTER_CAPACITY];
    Value * reg_float_alloced[BBAE_REGISTER_CAPACITY];
    
    memset(reg_int_alloced, 0, BBAE_REGISTER_CAPACITY * sizeof(Value *));
    reg_int_alloced[_ABI_RSP] = (Value *)-1;
    reg_int_alloced[_ABI_RBP] = (Value *)-1;
    reg_int_alloced[_ABI_R11] = (Value *)-1; // universal scratch register R11
    memset(reg_float_alloced, 0, BBAE_REGISTER_CAPACITY * sizeof(Value *));
    reg_float_alloced[_ABI_XMM5 - _ABI_XMM0] = (Value *)-1; // universal scratch register XMM5
    
    if (block == func->entry_block)
    {
        // allocate function arguments
        regalloc_func_args(func, reg_int_alloced, reg_float_alloced);
    }
    else
    {
        // allocate block arguments
        regalloc_block_args(block, reg_int_alloced, reg_float_alloced);
    }
    
    // give statements numbers for spill heuristic
    for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
        block->statements[i]->num = i;
    
    for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
    {
        Statement * statement = block->statements[i];
        
        // free no-longer-used registers, except for reserved ones (RSP, RBP, R11, XMM5)
        expire_unused_regs(reg_int_alloced, reg_float_alloced);
        
        // tick the usage count of the statement's operands
        for (size_t j = 0; j < array_len(statement->args, Operand); j++)
        {
            Value * arg = statement->args[j].value;
            if (arg && arg->variant != VALUE_CONST)
            {
                arg->alloced_use_count += 1;
                assert(arg->alloced_use_count <= array_len(arg->edges_out, Value *));
            }
        }
        
        // skip this statement if it doesn't have an output to allocate
        if (!statement->output)
            continue;
        
        RegAllocRules rules = regalloc_rule_determiner(statement);
        
        uint8_t is_special = rules.is_special;
        uint8_t is_int = type_is_intreg(statement->output->type);
        
        //printf("-- allocating reg TYPE %d for SSA var %s in block %s\n", is_int, statement->output_name, block->name);
        
        uint8_t op_is_commutative = 0;
        if (strcmp(statement->statement_name, "add") == 0 ||
            strcmp(statement->statement_name, "mul") == 0 ||
            strcmp(statement->statement_name, "imul") == 0 ||
            strcmp(statement->statement_name, "fadd") == 0 ||
            strcmp(statement->statement_name, "fmul") == 0)
            op_is_commutative = 1;
        
        // reuse an operand register if possible
        for (size_t j = 0; j < array_len(statement->args, Operand); j++)
        {
            if (j > 0 && !op_is_commutative)
                break;
            
            Value * arg = statement->args[j].value;
            if (arg && arg->regalloced
                && arg->alloced_use_count == array_len(arg->edges_out, Value *))
            {
                if (is_int != type_is_intreg(arg->type))
                    continue;
                
                if (is_special && ((~rules.allowed_output_registers >> arg->regalloc) & 1))
                    continue;
                
                statement->output->regalloc = arg->regalloc;
                statement->output->regalloced = 1;
                
                int64_t where = arg->regalloc;
                
                if (where >= _ABI_XMM0)
                    reg_float_alloced[where - _ABI_XMM0] = statement->output;
                else 
                    reg_int_alloced[where] = statement->output;
                
                goto early_continue;
            }
        }
        if (0)
        {
            early_continue:
            continue;
        }
        
        uint64_t allow_mask = is_special ? rules.allowed_output_registers : 0xFFFFFFFF;
        
        int64_t where;
        if (is_int)
            where = first_empty(reg_int_alloced, BBAE_REGISTER_COUNT, allow_mask);
        else
            where = first_empty(reg_float_alloced, BBAE_REGISTER_COUNT, allow_mask >> 16) + _ABI_XMM0;
        
        // spill a register away if needed
        if (where < 0)
        {
            int64_t to_spill_reg = -1;
            uint64_t to_spill_num = 0;
            find_spillable(statement, is_int, reg_int_alloced, reg_float_alloced, &to_spill_reg, &to_spill_num, rules.allowed_output_registers);
            assert(to_spill_reg >= 0);
            
            Value * spillee;
            if (is_int)
                spillee = reg_int_alloced[to_spill_reg];
            else
                spillee = reg_float_alloced[to_spill_reg - _ABI_XMM0];
            
            where = spillee->regalloc;
            
            printf("spilling %s...\n", spillee->ssa ? spillee->ssa->output_name : spillee->arg);
            
            do_spill(func, block, reg_int_alloced, reg_float_alloced, spillee, to_spill_reg, to_spill_num, (~rules.clobbered_registers) & (~(1 << where)), &i);
            
            if (is_special)
                assert((rules.allowed_output_registers >> where) & 1);
        }
        
        if (where >= _ABI_XMM0)
        {
            assert(reg_float_alloced[where - _ABI_XMM0] == 0);
            reg_float_alloced[where - _ABI_XMM0] = statement->output;
        }
        else
        {
            assert(reg_int_alloced[where] == 0);
            reg_int_alloced[where] = statement->output;
        }
        
        statement->output->regalloc = where;
        statement->output->regalloced = 1;
        
        if (is_int)
            assert(where >= 0 && where <= 15);
        else
            assert(where >= 16 && where <= 31);
        
        // spill clobbered registers
        if (is_special && rules.clobbered_registers)
        {
            for (uint64_t reg = 0; reg < 32; reg++)
            {
                uint8_t is_clobbered = (rules.clobbered_registers >> reg) & 1;
                if (!is_clobbered)
                    continue;
                
                Value * alloc;
                if (reg >= _ABI_XMM0)
                    alloc = reg_float_alloced[reg - _ABI_XMM0];
                else 
                    alloc = reg_int_alloced[reg];
                
                // don't want to do anything if the reg is used 
                if (alloc == statement->output)
                    continue;
                // no users
                if (array_len(alloc->edges_out, Statement *) == 0)
                    continue;
                // clobbered, but dies in or before current statement
                if (alloc->edges_out[array_len(alloc->edges_out, Statement *) - 1]->num <= statement->num)
                    continue;
                
                // only need to do anything for this reg if it's allocated
                if (alloc && alloc != (Value *)-1)
                    do_spill(func, block, reg_int_alloced, reg_float_alloced, alloc, reg, statement->num + 1, ~rules.clobbered_registers, &i);
            }
        }
    }
}

static void do_regalloc(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            do_regalloc_block(func, block);
            // FIXME
            // func->written_registers[where] = 1;
        }
    }
}

static ImmOpsAllowed imm_op_rule_determiner(Statement * statement)
{
    ImmOpsAllowed ret;
    memset(&ret, 1, sizeof(ImmOpsAllowed));
    if (strcmp(statement->statement_name, "add") == 0 ||
        strcmp(statement->statement_name, "sub") == 0)
    {
        ret.immediates_allowed[0] = 0;
    }
    else if (strcmp(statement->statement_name, "fadd") == 0 ||
        strcmp(statement->statement_name, "mul") == 0 ||
        strcmp(statement->statement_name, "imul") == 0 ||
        strcmp(statement->statement_name, "div") == 0 ||
        strcmp(statement->statement_name, "idiv") == 0 ||
        strcmp(statement->statement_name, "rem") == 0 ||
        strcmp(statement->statement_name, "irem") == 0 ||
        strcmp(statement->statement_name, "fsub") == 0 ||
        strcmp(statement->statement_name, "fmul") == 0 ||
        strcmp(statement->statement_name, "fxor") == 0 ||
        strcmp(statement->statement_name, "fdiv") == 0)
    {
        ret.immediates_allowed[0] = 0;
        ret.immediates_allowed[1] = 0;
    }
    else if (strcmp(statement->statement_name, "bitcast") == 0)
    {
        ret.immediates_allowed[0] = 0;
        if (type_is_float(statement->output->type))
            ret.immediates_allowed[1] = 0;
    }
    
    return ret;
}

#endif // BBAE_REGALLOC
