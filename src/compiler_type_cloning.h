#ifndef BBAE_COMPILER_TYPE_CLONING
#define BBAE_COMPILER_TYPE_CLONING

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "compiler_common.h"
#include "memory.h"

typedef struct _RemapInfo
{
    const void * oldptr;
    void * newptr;
} RemapInfo;

static void * remap_lookup(RemapInfo * info, const void * oldptr)
{
    for (size_t i = 0; i < array_len(info, RemapInfo); i++)
    {
        if (info[i].oldptr == oldptr)
            return info[i].newptr;
    }
    return 0;
}

static void remap_add(RemapInfo ** info, const void * oldptr, void * newptr)
{
    RemapInfo entry = { oldptr, newptr };
    array_push(*info, RemapInfo, entry);
}

static char * string_clone(RemapInfo ** info, const char * old);
static Operand operand_clone(RemapInfo ** info, Operand old);
static Statement * statement_clone(RemapInfo ** info, Statement * old);
static Block * block_clone(RemapInfo ** info, Block * old);
static StackSlot * slot_clone(RemapInfo ** info, StackSlot * old);
static Value * value_clone(RemapInfo ** info, Value * old);

static char * string_clone(RemapInfo ** info, const char * old)
{
    if (!old)
        return 0;
    if (remap_lookup(*info, old))
        return (char *)remap_lookup(*info, old);
    char * ret = strcpy_z(old);
    remap_add(info, old, ret);
    return ret;
}

static Operand operand_clone(RemapInfo ** info, Operand old)
{
    Operand ret = old;
    ret.value = value_clone(info, ret.value);
    ret.text = string_clone(info, ret.text);
    return ret;
}

static Statement * statement_clone(RemapInfo ** info, Statement * old)
{
    if (!old)
        return 0;
    if (remap_lookup(*info, old))
        return (Statement *)remap_lookup(*info, old);
    Statement * ret = new_statement();
    memcpy(ret, old, sizeof(Statement));
    remap_add(info, old, ret);
    
    ret->output_name = string_clone(info, ret->output_name);
    ret->statement_name = string_clone(info, ret->statement_name);
    ret->output = value_clone(info, ret->output);
    ret->block = block_clone(info, ret->block);
    
    ret->args = (Operand *)zero_alloc_clone(ret->args);
    
    for (size_t i = 0; i < array_len(ret->args, Operand); i++)
        ret->args[i] = operand_clone(info, ret->args[i]);
    
    return ret;
}

static Block * block_clone(RemapInfo ** info, Block * old)
{
    if (!old)
        return 0;
    if (remap_lookup(*info, old))
        return (Block *)remap_lookup(*info, old);
    Block * ret = new_block();
    memcpy(ret, old, sizeof(Block));
    remap_add(info, old, ret);
    
    ret->name = string_clone(info, ret->name);
    
    ret->args = (Value **)zero_alloc_clone(ret->args);
    ret->edges_in = (Statement **)zero_alloc_clone(ret->edges_in);
    ret->statements = (Statement **)zero_alloc_clone(ret->statements);
    
    for (size_t i = 0; i < array_len(ret->args, Value *); i++)
        ret->args[i] = value_clone(info, ret->args[i]);
    for (size_t i = 0; i < array_len(ret->edges_in, Statement *); i++)
        ret->edges_in[i] = statement_clone(info, ret->edges_in[i]);
    for (size_t i = 0; i < array_len(ret->statements, Statement *); i++)
        ret->statements[i] = statement_clone(info, ret->statements[i]);
    
    return ret;
}

static StackSlot * slot_clone(RemapInfo ** info, StackSlot * old)
{
    if (!old)
        return 0;
    if (remap_lookup(*info, old))
        return (StackSlot *)remap_lookup(*info, old);
    StackSlot * ret = (StackSlot *)zero_alloc(sizeof(StackSlot));
    memcpy(ret, old, sizeof(StackSlot));
    remap_add(info, old, ret);
    
    ret->name = string_clone(info, ret->name);
    
    return ret;
}

static Value * value_clone(RemapInfo ** info, Value * old)
{
    if (!old)
        return 0;
    if (remap_lookup(*info, old))
        return (Value *)remap_lookup(*info, old);
    Value * ret = make_value(old->type);
    memcpy(ret, old, sizeof(Value));
    remap_add(info, old, ret);
    
    ret->ssa = statement_clone(info, ret->ssa);
    ret->arg = string_clone(info, ret->arg);
    ret->slotinfo = slot_clone(info, ret->slotinfo);
    ret->spilled = slot_clone(info, ret->spilled);
    
    ret->edges_out = (Statement **)zero_alloc_clone(ret->edges_out);
    
    for (size_t i = 0; i < array_len(ret->edges_out, Statement *); i++)
        ret->edges_out[i] = statement_clone(info, ret->edges_out[i]);
    
    return ret;
}

static Function * func_clone(Function * oldfunc)
{
    if (!oldfunc)
        return 0;
    
    RemapInfo * info = (RemapInfo *)zero_alloc(0);
    Function * newfunc = (Function *)zero_alloc_clone(oldfunc);
    remap_add(&info, oldfunc, newfunc);
    
    newfunc->args = (Value **)zero_alloc_clone(newfunc->args);
    newfunc->blocks = (Block **)zero_alloc_clone(newfunc->blocks);
    newfunc->stack_slots = (Value **)zero_alloc_clone(newfunc->stack_slots);
    
    for (size_t a = 0; a < array_len(newfunc->args, Value *); a++)
        newfunc->args[a] = value_clone(&info, newfunc->args[a]);
    for (size_t i = 0; i < array_len(newfunc->blocks, Block *); i++)
        newfunc->blocks[i] = block_clone(&info, newfunc->blocks[i]);
    for (size_t i = 0; i < array_len(newfunc->stack_slots, Value *); i++)
        newfunc->stack_slots[i] = value_clone(&info, newfunc->stack_slots[i]);
    
    newfunc->name = string_clone(&info, newfunc->name);
    newfunc->entry_block = block_clone(&info, newfunc->entry_block);
    
    return newfunc;
}

#endif // BBAE_COMPILER_TYPE_CLONING
