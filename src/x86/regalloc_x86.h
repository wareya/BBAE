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

static int64_t first_empty(Value ** array, size_t len)
{
    // TODO: if block has no calls in it, prefer non-callee-saved registers
    int64_t found = -1;
    for (size_t n = 0; n < len; n++)
    {
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

static void do_regalloc_block(Function * func, Block * block)
{
    Value * reg_int_alloced[BBAE_REGISTER_CAPACITY];
    Value * reg_float_alloced[BBAE_REGISTER_CAPACITY];
    
    memset(reg_int_alloced, 0, BBAE_REGISTER_CAPACITY * sizeof(Value *));
    reg_int_alloced[_ABI_RSP] = (Value *)-1;
    reg_int_alloced[_ABI_RBP] = (Value *)-1;
    memset(reg_float_alloced, 0, BBAE_REGISTER_CAPACITY * sizeof(Value *));
    
    if (block == func->entry_block)
    {
        // special case for arguments
        abi_reset_state();
        for (size_t i = 0; i < array_len(func->args, Value *); i++)
        {
            Value * value = func->args[i];
            if (array_len(value->edges_out, Statement *) == 0)
                continue;
            if (type_is_basic(value->type))
            {
                int64_t where = abi_get_next_arg_basic(type_is_float(value->type));
                assert(("arg reg spill not yet supported", where >= 0));
                if (where >= _ABI_XMM0)
                    reg_float_alloced[where - _ABI_XMM0] = value;
                else 
                    reg_int_alloced[where] = value;
                
                printf("allocated register %zd to arg %s\n", where, value->arg);
                
                value->regalloc = where;
                value->regalloced = 1;
            }
            else
                assert(("aggregate args not yet supported", 0));
        }
    }
    else
    {
        // TODO: allocate based on output allocation of entry blocks
        assert(("TODO later block regalloc", 0));
    }
    
    // give statements numbers for regalloc reasons
    for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
        block->statements[i]->num = i;
    
    for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
    {
        Statement * statement = block->statements[i];
        
        // free no-longer-used registers, except for RSP/RBP
        // commented out to implement spilling
#ifndef BBAE_DEBUG_SPILLS
        for (size_t n = 0; n < BBAE_REGISTER_COUNT; n++)
        {
            if (reg_int_alloced[n] && reg_int_alloced[n] != (Value *)-1 && n != _ABI_RSP && n != _ABI_RBP)
            {
                Value * value = reg_int_alloced[n];
                assert(value->alloced_use_count <= array_len(value->edges_out, Value *));
                if (value->alloced_use_count == array_len(value->edges_out, Value *))
                {
                    printf("freeing int register %zu... %zu vs %zu\n", n, value->alloced_use_count, array_len(value->edges_out, Value *));
                    reg_int_alloced[n] = 0;
                }
            }
            if (reg_float_alloced[n] && reg_float_alloced[n] != (Value *)-1)
            {
                Value * value = reg_float_alloced[n];
                assert(value->alloced_use_count <= array_len(value->edges_out, Value *));
                if (value->alloced_use_count == array_len(value->edges_out, Value *))
                {
                    printf("freeing float register %zu...\n", n);
                    reg_float_alloced[n] = 0;
                }
            }
        }
#endif // BBAE_DEBUG_SPILLS
        
        for (size_t j = 0; j < array_len(statement->args, Operand); j++)
        {
            Value * arg = statement->args[j].value;
            if (arg && arg->variant != VALUE_CONST)
            {
                arg->alloced_use_count += 1;
                assert(arg->alloced_use_count <= array_len(arg->edges_out, Value *));
            }
        }
        
        if (!statement->output)
            continue;
        
        assert(("TODO", type_is_intreg(statement->output->type)));
        
        // reuse an operand register if possible
        for (size_t j = 0; j < array_len(statement->args, Operand); j++)
        {
            Value * arg = statement->args[j].value;
            if (arg && arg->regalloced
                && arg->alloced_use_count == array_len(arg->edges_out, Value *))
            {
                statement->output->regalloc = arg->regalloc;
                statement->output->regalloced = 1;
                
                int64_t where = arg->regalloc;
                
                if (where >= _ABI_XMM0)
                    reg_float_alloced[where - _ABI_XMM0] = statement->output;
                else 
                    reg_int_alloced[where] = statement->output;
                
                goto early_cont;
            }
        }
        if (0)
        {
            early_cont:
            continue;
        }
        
        int64_t where = first_empty(reg_int_alloced, BBAE_REGISTER_COUNT);
        if (where < 0)
        {
            // pick the register whose next use is the furthest into the future
            int64_t to_spill = -1;
            uint64_t to_spill_num = 0;
            for (size_t n = 0; n < BBAE_REGISTER_COUNT; n++)
            {
                Value * spill_candidate = 0;
                if (!(reg_int_alloced[n] && reg_int_alloced[n] != (Value *)-1 && n != _ABI_RSP && n != _ABI_RBP))
                    continue;
                
                spill_candidate = reg_int_alloced[n];
                
                // check if spill candidate is an input. skip if it is.
                for (size_t j = 0; j < array_len(statement->args, Operand); j++)
                {
                    if (statement->args[j].value == spill_candidate)
                        goto force_skip;
                }
                if (0)
                {
                    force_skip:
                    continue;
                }
                
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
                
                if (to_spill == -1 || candidate_last_num > to_spill_num)
                {
                    to_spill = n;
                    to_spill_num = candidate_last_num;
                    continue;
                }
            }
            assert(to_spill >= 0);
            
            Value * spillee = reg_int_alloced[to_spill];
            printf("want to spill reg %zu (%p)\n", to_spill, (void *)spillee);
            
            assert(spillee->regalloced);
            assert(spillee->regalloc >= 0);
            assert(!spillee->spilled);
            
            // spill statements don't need to be numbered because they're never an outward edge of an SSA value
            Value * spill_slot = add_stack_slot(func, make_temp_var_name(), type_size(spillee->type));
            spillee->spilled = spill_slot->slotinfo;
            
            Statement * spill = new_statement();
            spill->statement_name = strcpy_z("store");
            array_push(spill->args, Operand, new_op_val(spill_slot));
            array_push(spill->args, Operand, new_op_val(spillee));
            
            array_insert(block->statements, Statement *, i, spill);
            i += 1;
            
            where = spillee->regalloc;
            
            // insert a load and rewrite usages of the spilled variable
            uint8_t first = 1;
            Statement * unspill = 0;
            for (size_t j = 0; j < array_len(spillee->edges_out, Statement *); j++)
            {
                Statement * st = spillee->edges_out[j];
                //printf("checking for unspilling... %zu vs %zu\n", st->num, to_spill_num);
                if (st->num > to_spill_num)
                {
                    if (first)
                    {
                        printf("unspilling...\n");
                        
                        ptrdiff_t inst_index = ptr_array_find(block->statements, st);
                        assert(inst_index != -1);
                        
                        unspill = new_statement();
                        unspill->output_name = make_temp_var_name();
                        unspill->statement_name = strcpy_z("load");
                        unspill->output = make_value(spillee->type);
                        unspill->output->variant = VALUE_SSA;
                        
                        Operand op = new_op_val(spill_slot);
                        array_push(unspill->args, Operand, op);
                        connect_statement_to_operand(unspill, op);
                        
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
        
        func->written_registers[where] = 1;
        
        if (where >= _ABI_XMM0)
            reg_float_alloced[where - _ABI_XMM0] = statement->output;
        else 
            reg_int_alloced[where] = statement->output;
        
        //printf("allocating %zd to statement %zu (assigns to %s)\n", where, statement->id, statement->output_name);
        
        statement->output->regalloc = where;
        statement->output->regalloced = 1;
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
        }
    }
}

#endif // BBAE_REGALLOC
