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
        printf("    %s\n", instruction.text);
        offset += instruction.info.length;
        runtime_address += instruction.info.length;
    }
}

int main(int argc, char ** argv)
{
    if (argc < 2)
        return puts("please provide file"), 0;
    
    FILE * f = fopen(argv[1], "rb");
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek (f, 0, SEEK_SET);
    
    char * buffer = (char *)malloc(length+1);
    if (!buffer)
        puts("failed to allocate memory"), exit(-1);
    
    size_t n = fread(buffer, 1, length, f);
    assert(n == (size_t)length);
    buffer[length] = 0;
    fclose(f);
    
    Program * program = parse(buffer);
    do_optimization(program);
    byte_buffer * code = do_lowering(program);
    
    for (size_t i = 0; i < code->len; i++)
        printf("%02X ", code->data[i]);
    puts("");
    
    print_asm(code);
    
    uint8_t * jit_code = copy_as_executable(code->data, code->len);
    
    // suppress non-posix-compliant gcc function pointer casting warning
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    int (*jit_main) (int, int) = (int(*)(int, int))(void *)(jit_code);
#pragma GCC diagnostic pop
    
    assert(jit_main);
    uint32_t asdf = jit_main(5, 5);
    
    printf("output: %d (0x%X)\n", asdf, asdf);
    
    
    free_as_executable(jit_code);
    free_all_compiler_allocs();
    
    free(buffer);
    
    return 0;
}
