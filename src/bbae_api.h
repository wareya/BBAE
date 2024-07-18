#ifndef BBAE_API
#define BBAE_API

#include "compiler_common.h"
#include "bbae_construction.h"
#include "bbae_optimization.h"

// TODO (long term): support other platforms (e.g. arm, risc-v, llvm)
#include "x86/bbae_emission_x86.h"

static Program * parse(const char * buffer)
{
    Program * program = parse_file(buffer);
    connect_graphs(program);
    return program;
}

static void do_optimization(Program * program)
{
    //optimization_empty_block_removal(program);
    //optimization_mem2reg(program);
}

static byte_buffer * do_lowering(Program * program)
{
    do_regalloc(program);
    allocate_stack_slots(program);
    byte_buffer * code = compile_file(program);
    return code;
}

#endif // BBAE_API
