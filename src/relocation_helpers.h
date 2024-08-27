#ifndef BBAE_RELOCATION_HELPERS
#define BBAE_RELOCATION_HELPERS

#include "compiler_common.h"
#include "buffers.h"

// Only end-of-word-relative 8-bit and 32-bit relocations are supported.

static void add_relocation(NameUsageInfo ** usages, uint64_t loc, const char * name, uint8_t size)
{
    NameUsageInfo info = {loc, name, size};
    if (!*usages)
        *usages = (NameUsageInfo *)zero_alloc(0);
    
    array_push((*usages), NameUsageInfo, info);
}

static uint64_t reloc_illegal_dummy_value = 0xABADB007ABADB007;

static void apply_relocations(Program * program, NameUsageInfo * usages, byte_buffer * code, int64_t (*target_lookup_func)(const char *, void *), void * userdata)
{
    if (!usages)
        return;
    
    for (size_t u = 0; u < array_len(usages, NameUsageInfo); u++)
    {
        NameUsageInfo info = usages[u];
        int64_t target = target_lookup_func(info.name, userdata);
        if (target == (int64_t)reloc_illegal_dummy_value)
        {
            UnusedRelocation reloc = {info};
            array_push(program->unused_relocation_log, UnusedRelocation, reloc);
            continue;
        }
        int64_t rewrite_loc = info.loc;
        if (info.size == 4)
        {
            int64_t diff = target - (rewrite_loc + 4);
            assert(diff >= -2147483648 && diff <= 2147483647);
            assert(rewrite_loc + 4 <= (int64_t)code->len);
            memcpy(code->data + rewrite_loc, &diff, 4);
        }
        else if (info.size == 1)
        {
            int64_t diff = target - (rewrite_loc + 1);
            assert(diff >= -128 && diff <= 127);
            assert(rewrite_loc + 1 <= (int64_t)code->len);
            memcpy(code->data + rewrite_loc, &diff, 1);
        }
        else
            assert(((void)"TODO", 0));
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
static void apply_label_relocations(Program * program, byte_buffer * code, Function * func)
{
    apply_relocations(program, _emitter_label_usages, code, get_block_loc_or_assert, (void *)func);
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
static void apply_static_relocations(Program * _program, byte_buffer * code, Program * program)
{
    // allocate and align statics
    for (size_t i = 0; i < array_len(program->statics, StaticData); i++)
    {
        StaticData stat = program->statics[i];
        while (code->len % size_guess_align(type_size(stat.type)))
            byte_push(code, 0);
        
        stat.location = code->len;
        
        printf("allocating static %s at %zu\n", stat.name, stat.location);
        
        if (type_size(stat.type) <= 8)
            bytes_push(code, (uint8_t *)&stat.init_data_short, type_size(stat.type));
        else
            bytes_push(code, stat.init_data_long, type_size(stat.type));
        
        program->statics[i] = stat;
    }
    // apply relocations pointing at statics
    apply_relocations(_program, static_addr_relocations, code, get_static_or_assert, (void *)program);
}

static NameUsageInfo * _emitter_symbol_usages = 0;
static void add_symbol_relocation(uint64_t loc, const char * name, uint8_t size)
{
    add_relocation(&_emitter_symbol_usages, loc, name, size);
}
static int64_t get_symbol_loc_or_dummy(const char * name, void * _list)
{
    SymbolEntry * symbollist = (SymbolEntry *)_list;
    for (size_t i = 0; i < array_len(symbollist, SymbolEntry); i++)
    {
        SymbolEntry s = symbollist[i];
        if (strcmp(s.name, name) == 0)
            return s.loc;
    }
    return reloc_illegal_dummy_value;
}
static void apply_symbol_relocations(Program * program, byte_buffer * code, SymbolEntry * list)
{
    apply_relocations(program, _emitter_symbol_usages, code, get_symbol_loc_or_dummy, (void *)list);
}

#endif //BBAE_RELOCATION_HELPERS
