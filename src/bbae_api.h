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
    puts("----- BEFORE OPTIMIZATION -----");
    print_ir_to(0, program);
    optimization_empty_block_removal(program);
    optimization_global_mem2reg(program);
    optimization_unused_block_arg_removal(program);
    optimization_empty_block_removal(program);
    puts("----- AFTER OPTIMIZATION -----");
    print_ir_to(0, program);
    puts("-----                    -----");
}

static byte_buffer * do_lowering(Program * program)
{
    do_regalloc(program);
    puts("-----   AFTER REGALLOC   -----");
    print_ir_to(0, program);
    puts("-----                    -----");
    allocate_stack_slots(program);
    byte_buffer * code = compile_file(program);
    return code;
}

#endif // BBAE_API
