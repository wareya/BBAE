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
    
    //debug_print_grammar_points(grammar);
    
    auto tokens = tokenize(grammar, text2.data());
    
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
        printf("Parse failed. Expected one of:\n");
        for (auto & str : furthest_maybes)
            printf("  \033[92m%s\033[0m\n", str->data());
        if (furthest >= tokens.size())
            printf("At end of input stream.\n");
        else
        {
            auto token = tokens[furthest];
            printf("On line %zu at column %zu:\n", token->row, token->column);
            size_t i = token->line_index;
            while (i < text2.size() && text2[i] != '\n')
            {
                if (i == token->index)
                    printf("\033[91m");
                else if (i == token->index + token->text->size())
                    printf("\033[0m");
                putc(text2[i++], stdout);
            }
            printf("\033[0m");
            puts("");
            for (size_t n = 1; n < token->column; n++)
                putc(' ', stdout);
            printf("^---\n");
        }
    }
    
    return 0;
}
