#ifndef BBAE_OPTIMIZATION
#define BBAE_OPTIMIZATION

#include "compiler_common.h"

Operand * _remap_args(Value ** block_args, Operand * exit_args, Operand * entry_args)
{
    size_t entry_count = array_len(block_args, Value *);
    size_t exit_count = array_len(exit_args, Operand);
    
    Operand * args_copy = (Operand *)zero_alloc(0);
    array_push(args_copy, Operand, exit_args[0]);
    
    for (size_t i = 1; i < exit_count; i++)
    {
        Operand op = exit_args[i];
        for (size_t j = 0; j < entry_count; j++)
        {
            Value * block_arg = block_args[j];
            Operand entry_op = entry_args[j];
            if (op.value == block_arg)
            {
                op = entry_op;
                break;
            }
        }
        assert(memcmp(&op, &exit_args[i], sizeof(Operand)) != 0);
        array_push(args_copy, Operand, op);
    }
    
    return args_copy;
}

void _remap_args_span(Value ** block_args, Operand * exit_args, Statement * entry, size_t entry_label_offset)
{
    size_t block_arg_count = array_len(block_args, Value *);
    size_t raw_entry_count = array_len(entry->args, Operand);
    
    size_t offset = entry_label_offset + 1;
    
    // ensure non-broken jump source
    size_t entry_arg_count = 0;
    for (size_t i = offset; i < raw_entry_count && entry->args[i].variant != OP_KIND_SEPARATOR; i++)
        entry_arg_count += 1;
    assert(entry_arg_count == block_arg_count);
    
    Operand * new_args = _remap_args(block_args, exit_args, entry->args + offset);
    
    for (size_t i = 0; i < block_arg_count; i++)
    {
        disconnect_statement_from_operand(entry, entry->args[i + offset]);
        array_erase(entry->args, Operand, offset);
    }
    
    entry->args[entry_label_offset] = new_args[0];
    
    for (size_t i = 0; i < block_arg_count; i++)
    {
        Operand op = new_args[i + 1];
        array_insert(entry->args, Operand, offset + i, op);
        connect_statement_to_operand(entry, op);
    }
}
void optimization_empty_block_removal(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 1; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            if (array_len(block->statements, Statement *) != 1)
                continue;
            Statement * exit = block->statements[0];
            assert(exit);
            if (strcmp(exit->statement_name, "goto") == 0)
            {
                Block * target_block = find_block(func, exit->args[0].text);
                size_t target_block_in_edge_index = (size_t)-1;
                for (size_t i = 0; i < array_len(target_block->edges_in, Statement *); i++)
                {
                    if (target_block->edges_in[i] == exit)
                    {
                        target_block_in_edge_index = i;
                        break;
                    }
                }
                assert(target_block_in_edge_index != (size_t)-1);
                
                array_erase(func->blocks, Block *, b);
                
                for (size_t i = 1; i < array_len(exit->args, Operand); i++)
                {
                    if (exit->args[i].value)
                        exit->args[i].value->temp = i - 1;
                    disconnect_statement_from_operand(exit, exit->args[0]);
                }
                
                size_t block_arg_count = array_len(block->args, Value *);
                
                // FIXME make sure arguments get transferred properly even if they're in a different order
                for (size_t i = 0; i < array_len(block->edges_in, Statement *); i++)
                {
                    Statement * entry = block->edges_in[i];
                    if (strcmp(entry->statement_name, "goto") == 0)
                    {
                        assert(strcmp(entry->args[0].text, block->name) == 0);
                        assert(block_arg_count == array_len(entry->args, Operand) - 1);
                        _remap_args_span(block->args, exit->args, entry, 0);
                        target_block->edges_in[target_block_in_edge_index] = entry;
                    }
                    if (strcmp(entry->statement_name, "if") == 0)
                    {
                        // need to make sure the separator exists
                        size_t separator_index = find_separator_index(entry->args);
                        assert(separator_index);
                        
                        uint8_t filled_once = 0;
                        
                        if (strcmp(entry->args[1].text, block->name) == 0)
                        {
                            _remap_args_span(block->args, exit->args, entry, 1);
                            target_block->edges_in[target_block_in_edge_index] = entry;
                            filled_once = 1;
                        }
                        
                        // need to recalculate because it might have moved
                        separator_index = find_separator_index(entry->args);
                        assert(separator_index);
                        
                        if (strcmp(entry->args[separator_index + 1].text, block->name) == 0)
                        {
                            _remap_args_span(block->args, exit->args, entry, separator_index + 1);
                            if (!filled_once)
                                target_block->edges_in[target_block_in_edge_index] = entry;
                            else
                                array_insert(target_block->edges_in, Statement *, target_block_in_edge_index, entry);
                        }
                    }
                }
            }
        }
    }
}

size_t count_op_num_times_used(Operand * list, Value * value, size_t start, size_t count)
{
    size_t ret = 0;
    for (size_t i = start; i < start + count; i++)
    {
        if (list[i].value == value)
            ret += 1;
    }
    return ret;
}

void optimization_unused_block_arg_removal(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        uint8_t did_work = 1;
        while (did_work)
        {
            did_work = 0;
            for (size_t b = 1; b < array_len(func->blocks, Block *); b++)
            {
                Block * block = func->blocks[b];
                
                for (size_t a = 0; a < array_len(block->args, Value *); a++)
                {
                    Value * arg = block->args[a];
                    uint8_t non_jump_back_to_self_usage_exists = 0;
                    for (size_t i = 0; i < array_len(arg->edges_out, Statement *); i++)
                    {
                        Statement * statement = arg->edges_out[i];
                        if (strcmp(statement->statement_name, "goto") == 0 && a + 1 < array_len(statement->args, Operand))
                        {
                            if (strcmp(statement->args[0].text, block->name) != 0 ||
                                statement->args[a + 1].value != arg ||
                                count_op_num_times_used(statement->args, arg, 1, array_len(statement->args, Operand) - 1) != 1)
                            {
                                non_jump_back_to_self_usage_exists = 1;
                                break;
                            }
                        }
                        else if (strcmp(statement->statement_name, "if") == 0 && a + 2 < array_len(statement->args, Operand))
                        {
                            size_t separator_index = find_separator_index(statement->args);
                            assert(separator_index > 0);
                            
                            if (strcmp(statement->args[1].text, block->name) != 0 ||
                                statement->args[a + 2].value != arg ||
                                count_op_num_times_used(statement->args, arg, 2, separator_index - 2) != 1)
                            {
                                non_jump_back_to_self_usage_exists = 1;
                                break;
                            }
                            
                            if (separator_index + 2 + a < array_len(statement->args, Operand) &&
                                (strcmp(statement->args[separator_index + 1].text, block->name) != 0 ||
                                 statement->args[separator_index + 2 + a].value != arg ||
                                 count_op_num_times_used(statement->args, arg, separator_index + 2, array_len(statement->args, Operand) - separator_index - 1) != 1))
                            {
                                non_jump_back_to_self_usage_exists = 1;
                                break;
                            }
                        }
                        else
                        {
                            non_jump_back_to_self_usage_exists = 1;
                            break;
                        }
                    }
                    if (!non_jump_back_to_self_usage_exists || array_len(arg->edges_out, Statement *) == 0)
                    {
                        for (size_t i = 0; i < array_len(arg->edges_out, Statement *); i++)
                        {
                            Statement * statement = arg->edges_out[i];
                            if (strcmp(statement->statement_name, "goto") == 0)
                            {
                                if (strcmp(statement->args[0].text, block->name) == 0)
                                {
                                    disconnect_statement_from_operand(statement, statement->args[a + 1]);
                                    array_erase(statement->args, Operand, a + 1);
                                }
                            }
                            else if (strcmp(statement->statement_name, "if") == 0)
                            {
                                assert(a + 2 < array_len(statement->args, Operand));
                                if (strcmp(statement->args[1].text, block->name) != 0)
                                {
                                    disconnect_statement_from_operand(statement, statement->args[a + 2]);
                                    array_erase(statement->args, Operand, a + 2);
                                }
                                size_t separator_index = find_separator_index(statement->args);
                                assert(separator_index > 0);
                                assert(separator_index + 2 + a < array_len(statement->args, Operand));
                                if (strcmp(statement->args[separator_index + 1].text, block->name) != 0)
                                {
                                    disconnect_statement_from_operand(statement, statement->args[separator_index + 2 + a]);
                                    array_erase(statement->args, Operand, separator_index + 2 + a);
                                }
                            }
                        }
                        
                        array_erase(block->args, Value *, a);
                        did_work = 1;
                        
                        for (size_t i = 0; i < array_len(block->edges_in, Statement *); i++)
                        {
                            Statement * statement = block->edges_in[i];
                            
                            if (strcmp(statement->statement_name, "goto") == 0)
                            {
                                assert(strcmp(statement->args[0].text, block->name) == 0);
                                assert(a + 1 < array_len(statement->args, Operand));
                                disconnect_statement_from_operand(statement, statement->args[a + 1]);
                                array_erase(statement->args, Operand, a + 1);
                            }
                            else if (strcmp(statement->statement_name, "if") == 0)
                            {
                                if (strcmp(statement->args[1].text, block->name) == 0)
                                {
                                    assert(a + 2 < array_len(statement->args, Operand));
                                    disconnect_statement_from_operand(statement, statement->args[a + 2]);
                                    array_erase(statement->args, Operand, a + 2);
                                }
                                size_t separator_index = find_separator_index(statement->args);
                                assert(separator_index > 0);
                                if (strcmp(statement->args[separator_index + 1].text, block->name) == 0)
                                {
                                    assert(separator_index + 2 + a < array_len(statement->args, Operand));
                                    disconnect_statement_from_operand(statement, statement->args[separator_index + 2 + a]);
                                    array_erase(statement->args, Operand, separator_index + 2 + a);
                                }
                            }
                            else
                                assert(0);
                        }
                    }
                }
            }
        }
    }
}

void optimization_global_mem2reg(Program * program)
{
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        
        for (size_t i = 0; i < array_len(func->stack_slots, Value *); i++)
        {
            Value * slot = func->stack_slots[i];
            assert(slot->variant == VALUE_STACKADDR);
            
            // skip if this stack slot's address is ever used directly
            uint8_t type_set = 0;
            uint8_t ever_loaded = 0;
            uint8_t ever_stored = 0;
            Type type;
            for (size_t s = 0; s < array_len(slot->edges_out, Statement *); s++)
            {
                Statement * edge = slot->edges_out[s];
                if (strcmp(edge->statement_name, "load") == 0)
                {
                    ever_loaded = 1;
                    type = edge->output->type;
                    type_set = 1;
                }
                else if (strcmp(edge->statement_name, "store") == 0)
                    ever_stored = 1;
                else
                    goto full_continue;
            }
            // if the value is never loaded, we can eliminate it and all of its stores
            // TODO: do so instead of just skipping
            // TODO: implement volatile and make volatile stores count as loads
            if (!ever_loaded)
                continue;
            
            ever_stored = ever_stored + 0; // suppress unused variable warning
            
            assert(type_set);
            
            printf("---- stack slot type %d\n", type.variant);
            
            // rewrite all blocks (except the first) to take and pass the variable as an argument, while handling stores/loads
            const char * name;
            name = make_temp_name();
            for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
            {
                Block * block = func->blocks[b];
                
                Value * newval = make_value(type);
                // if entry block, insert original initialization instead of argument
                if (block == func->entry_block)
                {
                    Statement * init = new_statement();
                    init->block = block;
                    
                    newval->variant = VALUE_SSA;
                    newval->ssa = init;
                    
                    init->output_name = name;
                    init->output = newval;
                    init->statement_name = strcpy_z("mov");
                    // TODO: use poison value instead of 0?
                    Value * default_val = make_const_value(type.variant, 0);
                    Operand op = new_op_val(default_val);
                    array_push(init->args, Operand, op);
                    connect_statement_to_operand(init, op);
                    
                    array_insert(block->statements, Statement *, 0, init);
                }
                else
                {
                    newval->variant = VALUE_ARG;
                    newval->arg = name;
                    
                    array_push(block->args, Value *, newval);
                }
                
                for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
                {
                    Statement * statement = block->statements[i];
                    assert(statement);
                    if (strcmp(statement->statement_name, "load") == 0 && statement->args[1].value == slot)
                    {
                        statement->statement_name = strcpy_z("mov");
                        
                        disconnect_statement_from_operand(statement, statement->args[0]);
                        array_erase(statement->args, Operand, 0);
                        
                        disconnect_statement_from_operand(statement, statement->args[0]);
                        Operand op = new_op_val(newval);
                        statement->args[0] = op;
                        connect_statement_to_operand(statement, op);
                    }
                    else if (strcmp(statement->statement_name, "store") == 0 && statement->args[0].value == slot)
                    {
                        Value * newval_mutated = make_value(type);
                        
                        newval_mutated->variant = VALUE_SSA;
                        newval_mutated->ssa = statement;
                        
                        statement->output_name = make_temp_name();
                        statement->output = newval_mutated;
                        statement->statement_name = strcpy_z("mov");
                        
                        disconnect_statement_from_operand(statement, statement->args[0]);
                        array_erase(statement->args, Operand, 0);
                        
                        newval = newval_mutated;
                    }
                    else if (strcmp(statement->statement_name, "goto") == 0)
                    {
                        Operand op = new_op_val(newval);
                        array_push(statement->args, Operand, op);
                        connect_statement_to_operand(statement, op);
                    }
                    else if (strcmp(statement->statement_name, "if") == 0)
                    {
                        Operand op = new_op_val(newval);
                        array_push(statement->args, Operand, op);
                        connect_statement_to_operand(statement, op);
                        
                        Operand op2 = new_op_val(newval);
                        size_t separator_pos = find_separator_index(statement->args);
                        assert(separator_pos);
                        array_insert(statement->args, Operand, separator_pos, op2);
                        connect_statement_to_operand(statement, op2);
                    }
                }
            }
            
            array_erase(func->stack_slots, Value *, i);
            i -= 1;
            
            full_continue: {}
        }
    }
}

#endif // BBAE_OPTIMIZATION
