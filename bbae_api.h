#ifndef BBAE_API
#define BBAE_API

#include "compiler_common.h"
#include "bbae_construction.h"

static Program * parse(const char * buffer)
{
    Program * program = parse_file(buffer);
    connect_graphs(program);
    
    return program;
}

#endif // BBAE_API
