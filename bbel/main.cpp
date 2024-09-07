#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <iterator>
#include <fstream>

#include "grammar.hpp"
#include "compiler.hpp"
#include "../src/bbae_api_jit.h"

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;
    
    std::ifstream f("grammar.txt", std::ios::binary);
    std::vector<char> text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    text.push_back(0);
    
    std::ifstream f2("test.bbel", std::ios::binary);
    std::vector<char> text2((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
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
        auto s = std::string_view(text2.data(), text2.size());
        print_tokenization_error(tokens, s);
        return 0;
    }
    
    //for (auto n : tokens) printf("%s\n", n->text->data());
    
    auto asdf = parse_as(grammar, tokens, "program");
    
    if (!asdf)
    {
        auto s = std::string_view(text2.data(), text2.size());
        print_parse_error(tokens, s);
        return 0;
    }
    
    //print_AST(*asdf);
    
    Program * program = compile_root(*asdf);
    do_optimization(program);
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    int64_t (*jit_main) (int, int) = (int64_t(*)(int, int))(void *)(&jit_code[loc]);
#pragma GCC diagnostic pop
    
    assert(jit_main);
    int64_t main_output = jit_main(0, 0);
    printf("%zd\n", main_output);
    main_output = jit_main(17, 0);
    printf("%zd\n", main_output);
    
    jit_free(jitinfo);
    
    free_all_compiler_allocs();
    
    return 0;
}
