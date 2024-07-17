#ifndef BBAE_OPTIMIZATION
#define BBAE_OPTIMIZATION

#include "compiler_common.h"

void optimization_mem2reg(Program * program)
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
                        size_t separator_pos = 0;
                        for (size_t i = 2; i < array_len(statement->args, Operand); i++)
                        {
                            if (statement->args[i].variant == OP_KIND_SEPARATOR)
                            {
                                separator_pos = i;
                                array_insert(statement->args, Operand, i, op2);
                                break;
                            }
                        }
                        assert(separator_pos);
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
