#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "types.hpp"

#include "grammar.hpp"
#include "compiler.hpp"
#include "../src/bbae_api_jit.h"

//void asdf2() { std::vector<float> asdf; }
//void asdf2() { std::string asdf; }

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;
    
    auto f = fopen("grammar.txt", "rb");
    Vec<char> text;
    int c;
    while ((c = fgetc(f)) >= 0)
        text.push_back(c);
    text.push_back(0);
    
    auto f2 = fopen("test.bbel", "rb");
    Vec<char> text2;
    while ((c = fgetc(f2)) >= 0)
        text2.push_back(c);
    text2.push_back(0);
    
    auto grammar = load_grammar(text.data());
    
    //debug_print_grammar_points(grammar);
    
    auto tokens = tokenize(grammar, text2.data());
    
    if (tokens.size() == 0)
    {
        puts("Error: program is empty.");
        return 0;
    }
    if (tokens.back()->text == 0)
    {
        print_tokenization_error(tokens, String(text2.data()));
        puts("failed to tokenize");
        return 0;
    }
    
    for (auto n : tokens)
    {
        if (n->from_regex)
            printf("> %s (via %s)\n", n->text->data(), n->from_regex->str.data());
        else
            printf("> %s\n", n->text->data());
    }
    
    auto asdf = parse_as(grammar, tokens, "program");
    
    if (!asdf)
    {
        print_parse_error(tokens, String(text2.data()));
        puts("failed to parse");
        return 0;
    }
    
    //print_AST(*asdf);
    
    Program * program = compile_root(*asdf);
    do_optimization(program);
    
    /*
    SymbolEntry * _symbollist;
    auto bytes = do_lowering(program, &_symbollist);
    return 0;
    */
    
    JitOutput jitinfo = do_jit_lowering(program);
    SymbolEntry * symbollist = jitinfo.symbollist;
    uint8_t * jit_code = jitinfo.jit_code;
    
    if (symbollist == 0)
    {
        puts("produced no output. exiting");
        return 0;
    }
    
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
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
    int64_t (*jit_main) (int, int) = (int64_t(*)(int, int))(void *)(&jit_code[loc]);
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
    
    assert(jit_main);
    int64_t main_output = jit_main(0, 0);
    printf("%zd\n", main_output);
    main_output = jit_main(17, 0);
    printf("%zd\n", main_output);
    
    jit_free(jitinfo);
    
    free_all_compiler_allocs();
    
    return 0;
}
