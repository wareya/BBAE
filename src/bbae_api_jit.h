#ifndef BBAE_API_JIT
#define BBAE_API_JIT

#include "bbae_api.h"
#include "jitify.h"

void jit_relocate_globals(Program * program, uint8_t * jit_code, size_t code_len, uint8_t * jit_globals)
{
    // relocate unresolved symbols over jit_code
    for (size_t i = 0; i < array_len(program->unused_relocation_log, UnusedRelocation); i++)
    {
        UnusedRelocation relocation = program->unused_relocation_log[i];
        size_t reloc_loc = relocation.info.loc;
        size_t reloc_size = relocation.info.size;
        assert(reloc_loc + reloc_size <= code_len);
        assert(reloc_size == 1 || reloc_size == 4);
        int64_t diff = 0;
        
        GlobalData global_data = find_global(program, relocation.info.name);
        if (global_data.name != 0)
        {
            int64_t total_offset = (int64_t)(jit_code - jit_globals);
            
            diff = global_data.location - (reloc_loc + 4);
            diff -= total_offset;
        }
        else
            assert(((void)"TODO: check for other symbols like function names etc", 0));
        
        if (reloc_size == 4)
            assert(diff >= -2147483647 - 1 && diff <= 2147483647);
        else if (reloc_size == 1)
            assert(diff >= -128 && diff <= 127);
        
        memcpy(jit_code + reloc_loc, &diff, reloc_size);
    }
}

typedef struct _JitOutput
{
    byte_buffer * raw_code;
    SymbolEntry * symbollist;
    
    uint8_t * jit_globals;
    size_t jit_globals_len;
    
    uint8_t * jit_code;
    size_t jit_code_len;
} JitOutput;

JitOutput do_jit_lowering(Program * program)
{
    SymbolEntry * symbollist = 0;
    byte_buffer * code = do_lowering(program, &symbollist);
    assert(symbollist);
    assert(code);
    if (code->len == 0)
    {
        JitOutput ret;
        memset(&ret, 0, sizeof(JitOutput));
        return ret;
    }
    
    size_t jit_globals_len = program->globals_bytecount;
    uint8_t * jit_globals = alloc_near_executable(&jit_globals_len);
    memset(jit_globals, 0, jit_globals_len);
    
    size_t code_len = code->len;
    uint8_t * jit_code = alloc_near_executable(&code_len);
    memset(jit_code, 0, code_len);
    memcpy(jit_code, code->data, code->len);
    
    // yes, this should be code->len, not code_len
    jit_relocate_globals(program, jit_code, code->len, jit_globals);
    
    // yes, this should be code_len, not code->len
    mark_as_executable(jit_code, code_len);
    
    JitOutput ret;
    memset(&ret, 0, sizeof(JitOutput));
    
    ret.raw_code = code;
    ret.symbollist = symbollist;
    ret.jit_globals = jit_globals;
    ret.jit_globals_len = jit_globals_len;
    ret.jit_code = jit_code;
    ret.jit_code_len = code_len;
    
    return ret;
}

void jit_free(JitOutput jitinfo)
{
    free(jitinfo.raw_code->data);
    free_near_executable(jitinfo.jit_code, jitinfo.jit_code_len);
    free_near_executable(jitinfo.jit_globals, jitinfo.jit_globals_len);
    
    nullify_relocation_buffers();
}

#endif // BBAE_API_JIT
