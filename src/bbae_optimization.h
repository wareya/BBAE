#ifndef BBAE_OPTIMIZATION
#define BBAE_OPTIMIZATION

#include "compiler_common.h"
#include "compiler_type_cloning.h"

static Operand * _remap_args(Value ** block_args, Operand * exit_args, Operand * entry_args)
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

static void _remap_args_span(Value ** block_args, Operand * exit_args, Statement * entry, size_t entry_label_offset)
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
static void optimization_empty_block_removal(Program * program)
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
                        assert(separator_index != (size_t)-1);
                        
                        uint8_t filled_once = 0;
                        
                        if (strcmp(entry->args[1].text, block->name) == 0)
                        {
                            _remap_args_span(block->args, exit->args, entry, 1);
                            target_block->edges_in[target_block_in_edge_index] = entry;
                            filled_once = 1;
                        }
                        
                        // need to recalculate because it might have moved
                        separator_index = find_separator_index(entry->args);
                        assert(separator_index != (size_t)-1);
                        
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

static size_t count_op_num_times_used(Operand * list, Value * value, size_t start, size_t count)
{
    size_t ret = 0;
    for (size_t i = start; i < start + count; i++)
    {
        if (list[i].value == value)
            ret += 1;
    }
    return ret;
}

static void optimization_unused_block_arg_removal(Program * program)
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
                            assert(separator_index != (size_t)-1);
                            
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
                                assert(separator_index != (size_t)-1);
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
                                assert(separator_index != (size_t)-1);
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

static void optimization_global_mem2reg(Program * program)
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
            const char * name = make_temp_name();
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
                        assert(separator_pos != (size_t)-1);
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

static void func_recalc_statement_count(Function * func)
{
    func->statement_count = 0;
    for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
    {
        Block * block = func->blocks[b];
        func->statement_count += array_len(block->statements, Statement *);
    }
}

static void optimization_function_inlining(Program * program)
{
    // first, collect the number of statements in each function
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        func_recalc_statement_count(func);
    }
    // now, check for functions tha call smallish (< 50 statements) functions that don't call anything and inline them
    for (size_t f = 0; f < array_len(program->functions, Function *); f++)
    {
        Function * func = program->functions[f];
        for (size_t b = 0; b < array_len(func->blocks, Block *); b++)
        {
            Block * block = func->blocks[b];
            
            // First, figure out whether the block contains a call. We only need to do any other work if it does.
            size_t call_index = (size_t)-1;
            for (size_t i = 0; i < array_len(block->statements, Statement *); i++)
            {
                Statement * statement = block->statements[i];
                if (strcmp(statement->statement_name, "call") == 0 ||
                    strcmp(statement->statement_name, "call_eval") == 0)
                {
                    call_index = i;
                    break;
                }
            }
            if (call_index == (size_t)-1)
                continue;
            
            Statement * call = block->statements[call_index];
            
            Type return_type = basic_type(TYPE_NONE);
            if (strcmp(call->statement_name, "call_eval") == 0)
            {
                return_type = call->output->type;
                assert(return_type.variant != TYPE_NONE && return_type.variant != TYPE_INVALID);
            }
            
            // look up called function, skip if we can't inline
            Value * call_arg = call->args[0].value;
            assert(call_arg);
            // dynamically loaded, can't inline
            if (!call_arg->ssa)
                continue;
            // dynamically loaded, can't inline
            if (strcmp(call_arg->ssa->statement_name, "symbol_lookup") != 0 &&
                strcmp(call_arg->ssa->statement_name, "symbol_lookup_unsized") != 0)
                continue;
            Function * called_func = find_func(program, call_arg->ssa->args[0].text);
            // TODO: support inlining functions that perform calls. need to check for recursion.
            if (called_func->performs_calls)
                continue;
            // our inlining heuristic.
            if (called_func->statement_count > 100)
                continue;
            
            // Now we need to split the block at the function call statement.
            
            // collect which SSA values are actually needed by which latest instructions
            // so that we can pass along the right arguments to the secondary blocks
            const char ** output_names = (const char **)zero_alloc(0);
            Value ** outputs = (Value **)zero_alloc(0);
            uint64_t * output_latest_use = (uint64_t *)zero_alloc(0);
            
            Value ** args = (block == func->entry_block) ? func->args : block->args;
            
            // collect arg live ranges
            for (size_t i = 0; i < array_len(args, Value *); i++)
            {
                Value * arg = args[i];
                assert(arg->variant == VALUE_ARG);
                arg->temp = array_len(output_latest_use, uint64_t);
                array_push(output_names, const char *, arg->arg);
                array_push(outputs, Value *, arg);
                array_push(output_latest_use, uint64_t, (uint64_t)(int64_t)-1);
            }
            
            // collect SSA var live ranges
            for (size_t i = 0; i < call_index; i++)
            {
                // Unlike normal block splitting, we don't need to collect the full live range of each SSA var.
                // We only need to know if the SSA var dies in the top half of the block or the bottom half.
                // This is because we're definitely using optimizations, so we don't need to avoid emitting unnecessary SSA block arguments
                //  because we have an optimization that removes them.
                assert(i < array_len(block->statements, Statement *));
                
                Statement * statement = block->statements[i];
                
                if (statement->output)
                {
                    assert(statement->output_name);
                    assert(statement->output->variant == VALUE_SSA);
                    statement->output->temp = array_len(output_latest_use, uint64_t);
                    array_push(output_names, const char *, statement->output_name);
                    array_push(outputs, Value *, statement->output);
                    array_push(output_latest_use, uint64_t, (uint64_t)(int64_t)-1);
                }
                
                for (size_t j = 0; j < array_len(statement->args, Operand); j++)
                {
                    Value * arg = statement->args[j].value;
                    if (arg && (arg->variant == VALUE_ARG || arg->variant == VALUE_SSA))
                    {
                        uint64_t k = arg->temp;
                        if (outputs[k] == arg)
                            output_latest_use[k] = i;
                    }
                }
            }
            
            // finally, actually split the block
            
            Block * next_block = new_block();
            next_block->name = make_temp_name();
            next_block->statements = array_chop(block->statements, Statement *, call_index+1);
            
            printf("splitting block %s at instruction %zu\n", block->name, call_index);
            printf("left len: %zu\n", array_len(block->statements, Statement *));
            printf("right len: %zu\n", array_len(next_block->statements, Statement *));
            
            // For the sake of simplicity we dump every cross-call live SSA variable to a stack slot.
            // This way we don't have to rewrite all the blocks in the inlined function.
            size_t cutoff = call_index;
            for (size_t i = 0; i < array_len(output_latest_use, uint64_t); i++)
            {
                size_t latest = output_latest_use[i];
                if (latest <= cutoff)
                    continue;
                
                Value * var = outputs[i];
                Value * output_slot = add_stack_slot(func, string_concat(string_concat(make_temp_name(), "_"), output_names[i]), type_size(var->type));
                
                Statement * store = new_statement();
                store->statement_name = strcpy_z("store");
                array_push(store->args, Operand, new_op_val(output_slot));
                connect_statement_to_operand(store, array_last(store->args, Operand));
                array_push(store->args, Operand, new_op_val(outputs[i]));
                connect_statement_to_operand(store, array_last(store->args, Operand));
                
                array_insert(block->statements, Statement *, call_index, store);
                call_index += 1;
                
                Statement * load = new_statement();
                load->statement_name = strcpy_z("load");
                load->output_name = output_names[i];
                array_push(load->args, Operand, new_op_type(var->type));
                connect_statement_to_operand(load, array_last(load->args, Operand));
                array_push(load->args, Operand, new_op_val(output_slot));
                connect_statement_to_operand(load, array_last(load->args, Operand));
                add_statement_output(load);
                
                array_insert(next_block->statements, Statement *, 0, load);
                
                Value * load_output = load->output;
                
                for (size_t j = 0; j < array_len(next_block->statements, Statement *); j++)
                {
                    Statement * statement = next_block->statements[j];
                    for (size_t n = 0; n < array_len(statement->args, Operand); n++)
                    {
                        if (statement->args[n].variant == OP_KIND_VALUE && statement->args[n].value == outputs[i])
                        {
                            disconnect_statement_from_operand(statement, statement->args[n]);
                            statement->args[n].value = load_output;
                            connect_statement_to_operand(statement, statement->args[n]);
                        }
                    }
                }
            }
            
            array_insert(func->blocks, Block *, b + 1, next_block);
            
            // clone inlined func body so that we can can rewrite its blocks
            Function * cloned = func_clone(called_func);
            
            // rewrite entry block to use block args instead of func args
            assert(cloned->entry_block);
            cloned->entry_block->args = cloned->args;
            cloned->args = (Value **)zero_alloc(0);
            
            // need a unique prefix for some later actions
            const char * name_prefix = string_concat(make_temp_name(), "_");
            
            // move stack slots into outer function
            for (size_t s = 0; s < array_len(cloned->stack_slots, Value *); s++)
            {
                Value * slotval = cloned->stack_slots[s];
                StackSlot * slotinfo = slotval->slotinfo;
                slotinfo->name = string_concat(name_prefix, slotinfo->name);
                array_push(func->stack_slots, Value *, cloned->stack_slots[s]);
            }
            cloned->stack_slots = 0;
            
            // invasive rewrites:
            // - add a prefix to block names, rewrite if and goto to use prefixed block names
            // - rewrite returns to jump out to the next block instead
            for (size_t b = 0; b < array_len(cloned->blocks, Block *); b++)
            {
                Block * rw_block = cloned->blocks[b];
                rw_block->name = string_concat(name_prefix, called_func->name);
                
                for (size_t s = 0; s < array_len(rw_block->statements, Statement *); s++)
                {
                    Statement * statement = rw_block->statements[s];
                    
                    if (strcmp(statement->statement_name, "return") == 0)
                    {
                        statement->statement_name = strcpy_z("goto");
                        if (return_type.variant == TYPE_NONE)
                            statement->args = (Operand *)zero_alloc(0);
                        array_insert(statement->args, Operand, 0, new_op_text(next_block->name));
                    }
                    else if (strcmp(statement->statement_name, "if") == 0 ||
                             strcmp(statement->statement_name, "goto") == 0)
                    {
                        for (size_t i = 0; i < array_len(statement->args, Operand); i++)
                        {
                            if (statement->args[i].variant == OP_KIND_TEXT)
                                statement->args[i].text = string_concat(name_prefix, statement->args[i].text);
                        }
                    }
                }
            }
            
            // if call_eval, replace output edges with block argument
            if (strcmp(call->statement_name, "call_eval") == 0)
            {
                assert(call->output);
                assert(call->output_name);
                Value * argval = make_value(return_type);
                argval->variant = VALUE_ARG;
                argval->arg = call->output_name;
                array_push(next_block->args, Value *, argval);
                
                for (size_t e = 0; e < array_len(call->output->edges_out, Statement *); e++)
                {
                    Statement * edge = call->output->edges_out[e];
                    for (size_t i = 0; i < array_len(edge->args, Operand); i++)
                    {
                        if (edge->args[i].value == call->output)
                        {
                            disconnect_statement_from_operand(edge, edge->args[i]);
                            edge->args[i].value = argval;
                            connect_statement_to_operand(edge, edge->args[i]);
                        }
                    }
                }
            }
            
            // replace call with goto
            call->output = 0;
            call->output_name = 0;
            call->statement_name = strcpy_z("goto");
            call->args[0] = new_op_text(cloned->entry_block->name);
            
            // insert blocks from inlined function into outer function
            for (size_t i = 0; i < array_len(cloned->blocks, Block *); i++)
                array_insert(func->blocks, Block *, b + i + 1, cloned->blocks[i]);
            
        }
    }
}

#endif // BBAE_OPTIMIZATION
