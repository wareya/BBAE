#include "my_regex.h"

int main(void)
{
    RegexToken tokens[1024];
    int16_t token_count = 1024;
    //int e = regex_parse("[0-9]+\\.[0-9]+", tokens, &token_count, 0);
    //int e = regex_parse("[0-9]+0\\.[0-9]+", tokens, &token_count, 0);
    
    //int e = regex_parse("(a|a|ab)bc", tokens, &token_count, 0);
    //int e = regex_parse("(ab|ab|a)bc", tokens, &token_count, 0);
    //int e = regex_parse("[0-9]\\.[0-9]", tokens, &token_count, 0);
    
    // lazy first group
    //int e = regex_parse("([0-9])*?\\.([0-9])+", tokens, &token_count, 0);
    // greedy first group
    //int e = regex_parse("([0-9]){3,5}?\\.([0-9])+", tokens, &token_count, 0);
    //int e = regex_parse("([0-9]){3,5}\\.([0-9])+", tokens, &token_count, 0);
    //int e = regex_parse("a(|)a", tokens, &token_count, 0);
    //int e = regex_parse("(a|ab)*b", tokens, &token_count, 0);
    //int e = regex_parse("(ab?)*?b", tokens, &token_count, 0);
    //int e = regex_parse("(ab?\?)*b", tokens, &token_count, 0);
    //int e = regex_parse("(a)?\?(b|a)", tokens, &token_count, 0);
    //int e = regex_parse("(a|)*b", tokens, &token_count, 0);
    //int e = regex_parse("a(|b)+?a", tokens, &token_count, 0);
    //int e = regex_parse("(a|b)*?b", tokens, &token_count, 0);
    //int e = regex_parse("a*a*?", tokens, &token_count, 0);
    //int e = regex_parse("a*?a*", tokens, &token_count, 0);
    //int e = regex_parse("(b|a)*b", tokens, &token_count, 0);
    //int e = regex_parse("(b|a)*?b", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|)*bb", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|)*?bb", tokens, &token_count, 0);
    //int e = regex_parse("(|a)+", tokens, &token_count, 0);
    //int e = regex_parse("()+", tokens, &token_count, 0);
    //int e = regex_parse("()+?", tokens, &token_count, 0);
    //int e = regex_parse("(|)+?", tokens, &token_count, 0);
    //int e = regex_parse("a(|)*a", tokens, &token_count, 0);
    //int e = regex_parse("a(|)*?a", tokens, &token_count, 0);
    //int e = regex_parse("(a|(((()))))*b", tokens, &token_count, 0);
    
    //int e = regex_parse("(b|a|)*+", tokens, &token_count, 0);
    int e = regex_parse("(?>(b|a|)*)", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|)*+b", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|as|q)*", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|as|q)*X", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|as|q)*+", tokens, &token_count, 0);
    //int e = regex_parse("(b|a|as|q)*+X", tokens, &token_count, 0);
    
    if (e) return (puts("regex has error"), 0);
    print_regex_tokens(tokens);
    
    /*
    int64_t match_len = regex_match(tokens, "abc) ");
    printf("########### return: %zd\n", match_len);
    match_len = regex_match(tokens, "abbc) ");
    printf("########### return: %zd\n", match_len);
    */
    /*
    int64_t match_len = regex_match(tokens, "012.53) ");
    printf("########### return: %zd\n", match_len);
    match_len = regex_match(tokens, ".53) ");
    printf("########### return: %zd\n", match_len);
    match_len = regex_match(tokens, "5.5");
    printf("########### return: %zd\n", match_len);
    */
    //int64_t match_len = regex_match(tokens, "022134.53) ");
    //int64_t match_len = regex_match(tokens, "02234.53) ");
    //int64_t match_len = regex_match(tokens, "1131.53) ");
    //int64_t match_len = regex_match(tokens, "131.53) ");
    //int64_t match_len = regex_match(tokens, "11.53) ");
    //int64_t match_len = regex_match(tokens, "1.53) ");
    //int64_t match_len = regex_match(tokens, "aa");
    int64_t match_len = regex_match(tokens, "aaaaaaaaababababb");
    //int64_t match_len = regex_match(tokens, "aaaaabbbbbbbx");
    //int64_t match_len = regex_match(tokens, "bbbbbbb");
    //int64_t match_len = regex_match(tokens, "aba");
    //int64_t match_len = regex_match(tokens, "asqbX");
    printf("########### return: %zd\n", match_len);
    
    return 0;
}
