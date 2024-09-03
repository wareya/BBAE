#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <iterator>
#include <fstream>

#include "grammar.hpp"

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
    
    auto tokens = tokenize(grammar, text2.data());
    
    for (auto n : tokens)
    {
        printf("%s\n", n.text.data());
    }
    
    return 0;
}
