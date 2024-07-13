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

#endif // BBAE_API
