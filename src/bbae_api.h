#ifndef BBAE_API
#define BBAE_API

#include "compiler_common.h"
#include "bbae_construction.h"

// TODO (long term): support other platforms (arm, risc-v, llvm)
#include "x86/bbae_emission_x86.h"

static Program * parse(const char * buffer)
{
    Program * program = parse_file(buffer);
    connect_graphs(program);
    return program;
}

static void do_optimization(Program * program)
{
    // suppress unused parameter and variable warnings
    Program * p = program;
    program = p;
    // TODO
}

static byte_buffer * do_lowering(Program * program)
{
    do_regalloc(program);
    allocate_stack_slots(program);
    byte_buffer * code = compile_file(program);
    return code;
}

#endif // BBAE_API
