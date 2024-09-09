#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "memory.h"
#include "bbae_api_jit.h"

uint64_t compile_and_run(const char * fname, uint64_t arg, uint8_t with_double)
{
    FILE * f = fopen(fname, "rb");
    
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
    
    assert(symbollist);
    
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
    
    // suppress non-posix-compliant gcc function pointer casting warning
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    uint64_t (*jit_main)(uint64_t) = (uint64_t(*)(uint64_t))(void *)(&jit_code[loc]);
    double (*jit_main_double)(uint64_t) = (double(*)(uint64_t))(void *)(&jit_code[loc]);
#pragma GCC diagnostic pop
    
    assert(jitinfo.raw_code);
    assert(jit_main);
    
    uint64_t jit_output;
    if (!with_double)
        jit_output = jit_main(arg);
    else
    {
        double temp = jit_main_double(arg);
        memcpy(&jit_output, &temp, 8);
    }
    
    
    assert(jitinfo.raw_code);
    
    jit_free(jitinfo);
    free_all_compiler_allocs();
    free(buffer);
    
    printf("%zu\n", jit_output);
    return jit_output;
}

#define TEST_XMM(X, T, V) { \
    uint64_t n = compile_and_run(X, 0, 1); \
    T val; \
    memcpy(&val, &n, sizeof(T)); \
    assert(val == V); \
    fprintf(stderr, "%s: %.20f -- pass!\n", X, val); \
}
#define TEST_RAX(X, T, V) { \
    uint64_t n = compile_and_run(X, 0, 0); \
    T val = *(T*)&n; \
    assert(val == V); \
    fprintf(stderr, "%s: %zd -- pass!\n", X, val); \
}
#define TEST_TERMINATES_SUCCESSFULLY(X, T, V) { \
    uint64_t n = compile_and_run(X, 0, 0); \
    T val = *(T*)&n; \
    fprintf(stderr, "%s: (finished running) -- pass!\n", X, val); \
}

#ifdef _WIN32
#define NULL_DEVICE "NUL:"
#else
#define NULL_DEVICE "/dev/null"
#endif

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;
    FILE * n = freopen(NULL_DEVICE, "w", stdout);
    (void)n;
    TEST_XMM("examples/gravity.bbae", double, 489999999.990911543369293212890625);
    return 0;
}
