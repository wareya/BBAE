#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#define BBAE_DEBUG_SPILLS

#include "memory.h"
#include "bbae_api_jit.h"

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
    JitOutput jitinfo = do_jit_lowering(program);
    SymbolEntry * symbollist = jitinfo.symbollist;
    uint8_t * jit_code = jitinfo.jit_code;
    
    if (symbollist == 0)
    {
        puts("produced no output. exiting");
        return 0;
    }
    
    for (size_t i = 0; i < jitinfo.raw_code->len; i++)
        printf("%02X ", jitinfo.raw_code->data[i]);
    puts("");
    
    print_asm(jitinfo.raw_code);
    
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
    double (*jit_main) (int, int) = (double(*)(int, int))(void *)(&jit_code[loc]);
#pragma GCC diagnostic pop
    
    assert(jit_main);
    double jit_output = jit_main(0, 0);
    printf("%f\n", jit_output);
    printf("%d\n", *(uint32_t*)&jit_output);
    jit_output = jit_main(0, 0);
    printf("%f\n", jit_output);
    printf("%d\n", *(uint32_t*)&jit_output);
    
    jit_free(jitinfo);
    
    free_all_compiler_allocs();
    
    free(buffer);
    
    return 0;
}
