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
    
    //print_asm(jitinfo.jit_code, jitinfo.raw_code->len);
    
    // suppress non-posix-compliant gcc function pointer casting warning
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    uint64_t (*jit_main)(uint64_t) = (uint64_t(*)(uint64_t))(void *)(&jit_code[loc]);
    double (*jit_main_double)(uint64_t) = (double(*)(uint64_t))(void *)(&jit_code[loc]);
#pragma GCC diagnostic pop
    
    assert(jitinfo.raw_code);
    assert(jit_main);
    assert(jit_main_double);
    
    uint64_t jit_output;
    if (!with_double)
        jit_output = jit_main(arg);
    else
    {
        double temp = jit_main_double(arg);
        //double temp = 0.0;
        memcpy(&jit_output, &temp, 8);
    }
    
    assert(jitinfo.raw_code);
    
    jit_free(jitinfo);
    free_all_compiler_allocs();
    free(buffer);
    
    printf("%zu\n", jit_output);
    return jit_output;
}

#ifdef _WIN32
#define NULL_DEVICE "NUL:"
#define STDOUT_DEVICE "CON"
#else
#define NULL_DEVICE "/dev/null"
#define STDOUT_DEVICE "/dev/stdout"
#endif

#ifdef _WIN32
#define CLOSE_STDOUT { \
    FILE * n = freopen(NULL_DEVICE, "w", stdout); \
    (void)n; }
#else
#include <unistd.h>
static int _old_stdout = 0;
#define CLOSE_STDOUT { \
    _old_stdout = dup(0); \
    FILE * n = freopen(NULL_DEVICE, "w", stdout); \
    (void) n; }
#endif

#ifdef _WIN32
#define REOPEN_STDOUT { FILE * n = freopen(STDOUT_DEVICE, "w", stdout); (void)n; }
#else
//#define REOPEN_STDOUT { freopen(STDOUT_DEVICE, "w", stdout); }
#define REOPEN_STDOUT { stdout = fdopen(_old_stdout, "w"); }
//#define REOPEN_STDOUT { dup2(_old_stdout, 0); }
#endif

/*
#undef CLOSE_STDOUT
#define CLOSE_STDOUT ;
#undef REOPEN_STDOUT
#define REOPEN_STDOUT ;
*/

#define TEST_XMM(X, T, V) { \
    CLOSE_STDOUT; \
    uint64_t n = compile_and_run(X, 0, 1); \
    T val; \
    REOPEN_STDOUT; \
    memcpy(&val, &n, sizeof(T)); \
    assert(val == V); \
    printf("%s: %.20f -- pass!\n", X, val); \
}
#define TEST_RAX(X, T, V) { \
    CLOSE_STDOUT; \
    uint64_t n = compile_and_run(X, 0, 0); \
    T val = *(T*)&n; \
    REOPEN_STDOUT; \
    assert(val == V); \
    printf("%s: %zd -- pass!\n", X, val); \
}
#define TEST_RUNS(X) { \
    CLOSE_STDOUT; \
    compile_and_run(X, 0, 0); \
    REOPEN_STDOUT; \
    printf("%s: (finished running) -- pass!\n", X); \
}

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;
    
    TEST_XMM("tests/retsanitytest.bbae", double, 0.0);
    TEST_RAX("tests/retsanitytest_int.bbae", uint64_t, 0);
    
    TEST_XMM("tests/optsanitytest.bbae", double, 0.0);
    TEST_XMM("examples/gravity.bbae", double, 4899999.999928221106529235839844);
    TEST_XMM("tests/gravtest.bbae", double, 4899999.999928221106529235839844);
    TEST_XMM("tests/loopsanity.bbae", double, 0.0);
    // this pi calculation doesn't fully finish, inaccuracy from actual pi is expected
    TEST_XMM("examples/too_simple.bbae", double, 3.141592652588050427198141);
    
    TEST_RAX("examples/fib.bbae", uint64_t, 433494437);
    
    TEST_RUNS("examples/global.bbae");
    
    puts("Tests finished!");
    fflush(stdout);
    return 0;
}
