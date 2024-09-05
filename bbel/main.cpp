#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <iterator>
#include <fstream>

#include "grammar.hpp"

#include "../src/bbae_builder.h"

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
    
    for (auto n : tokens)
    {
        //printf("%s\n", n->text->data());
    }
    
    auto asdf = parse_as(grammar, tokens, "program");
    //printf("%d\n", !!asdf);
    //printf("%zu\n", furthest);
    
    if (asdf)
        print_AST(*asdf);
    else
    {
        auto s = std::string_view(text2.data(), text2.size());
        print_parse_error(tokens, s);
    }
    
    return 0;
}
