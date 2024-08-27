#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#define BBAE_DEBUG_SPILLS

#include "memory.h"
#include "bbae_api.h"
#include "jitify.h"

void print_asm(byte_buffer * code)
{
    //FILE * logfile = 0;
    
    size_t offset = 0; 
    size_t runtime_address = 0; 
    ZydisDisassembledInstruction instruction; 
    while (ZYAN_SUCCESS(ZydisDisassembleIntel(
        ZYDIS_MACHINE_MODE_LONG_64,
        runtime_address,
        code->data + offset,  // buffer
        code->len - offset,   // length
        &instruction
    )))
    {
        printf("0x%04zX    %s\n", offset, instruction.text);
        offset += instruction.info.length;
        runtime_address += instruction.info.length;
    }
}

void print_double(double f)
{
    printf("%.9f\n", f);
}

int main(int argc, char ** argv)
{
    if (argc < 2)
        return puts("please provide file"), 0;
    
    FILE * f = fopen(argv[1], "rb");
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char * buffer = (char *)malloc(length+1);
    if (!buffer)
        puts("failed to allocate memory"), exit(-1);
    
    size_t n = fread(buffer, 1, length, f);
    assert(n == (size_t)length);
    buffer[length] = 0;
    fclose(f);
    
    Program * program = parse(buffer);
    do_optimization(program);
    
    SymbolEntry * symbollist = 0;
    byte_buffer * code = do_lowering(program, &symbollist);
    assert(symbollist);
    
    assert(code);
    if (code->len == 0)
    {
        puts("produced no output. exiting");
        return 0;
    }
    
    for (size_t i = 0; i < code->len; i++)
        printf("%02X ", code->data[i]);
    puts("");
    
    print_asm(code);
    
    size_t globals_len = program->globals_bytecount;
    uint8_t * jit_globals = alloc_near_executable(&globals_len);
    memset(jit_globals, 0, globals_len);
    
    size_t code_len = code->len;
    uint8_t * jit_code = alloc_near_executable(&code_len);
    memset(jit_code, 0, code_len);
    memcpy(jit_code, code->data, code->len);
    
    // relocate unresolved symbols over jit_code
    for (size_t i = 0; i < array_len(program->unused_relocation_log, UnusedRelocation); i++)
    {
        UnusedRelocation relocation = program->unused_relocation_log[i];
        size_t reloc_loc = relocation.info.loc;
        size_t reloc_size = relocation.info.size;
        assert(reloc_loc + reloc_size <= code->len);
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
            assert(diff >= -2147483648 && diff <= 2147483647);
        else if (reloc_size == 1)
            assert(diff >= -128 && diff <= 127);
        
        memcpy(jit_code + reloc_loc, &diff, reloc_size);
        
        //printf("need to relocate: %s\n", relocation.info.name);
    }
    
    mark_as_executable(jit_code, code_len);
    
    ptrdiff_t loc = -1;
    for (size_t i = 0; symbollist[i].name; i++)
    {
        if (strcmp(symbollist[i].name, "main") == 0)
        {
            loc = symbollist[i].loc;
            break;
        }
    }
    assert(loc >= 0);
    
    printf("-- %p\n", (void *)jit_code);
    printf("-- %p\n", (void *)main);
    
    // suppress non-posix-compliant gcc function pointer casting warning
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    int64_t (*jit_main) (int, int) = (int64_t(*)(int, int))(void *)(&jit_code[loc]);
#pragma GCC diagnostic pop
    
    assert(jit_main);
    int64_t asdf = jit_main(0, 0);
    printf("%zd\n", asdf);
    asdf = jit_main(0, 0);
    printf("%zd\n", asdf);
    
    free_as_executable(jit_code, code_len);
    free_all_compiler_allocs();
    
    free(buffer);
    
    return 0;
}
