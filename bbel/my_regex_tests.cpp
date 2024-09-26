//#define REGEX_VERBOSE
#include "my_regex.h"

#include <regex>
#include <chrono>

void testify(void)
{
    static const char * regexes[] = {
        // (?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\])
        "(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|\"(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21\\x23-\\x5b\\x5d-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])*\")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21-\\x5a\\x53-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])+)\\])",
        //"(?:\\w+(?:\\.\\w+)*)@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)",
        //"(?:\\w+(?:\\.\\w+)*)@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)",
        "((\\w+)?\\.)+",
        //"(\\w\\w*\\.)+",
        //"(\\w+\\.)+",
        //"(?:\\w+(?:\\.\\w+)*)@(?:\\w+(?:\\.\\w+)*)",
        "[a-z0-9\\._%+!$&*=^|~#%'`?{}/\\-]+@([a-z0-9\\-]+\\.){1,}([a-z]{2,16})",
        "^[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}$",
        
        "(ab?)b",
        "(ab?)*b",
        "(ab?)*?b",
        
        "([0a-z][a-z0-9]*,)+",
        "([a-z][a-z0-9]*,)+",
        
        "asdf\\b",
        "asdf\\B",
        "\\basdf",
        
        "(\\ba?)*",
        "(\\ba?)*?",
        "(\\b)+?",
        "(\\b)+",
        "(\\ba?)+",
        "(\\ba?)+?",
        "(\\ba?)*a",
        "(\\ba?)*?a",
        "(\\ba?)+a",
        "(\\ba?)+?a",
        "a(\\b)*",
        "a(\\b)*?",
        "(\\b)*a",
        "(\\b)*?a",
        "a(\\b)+",
        "a(\\b)+?",
        "(\\b)+a",
        "(\\b)+?a",
        
        "^asdf$",
        "^asdf",
        "asdf$",
        ".*asdf",
        ".*asdf$",
        
        "(^(asdf)?)*",
        "(^(asdf)?)*(asdf)?",
        "((asdf)?$)*",
        "((asdf)?)*((asdf)?$)*",
        "(^(asdf)?)*?",
        "(^(asdf)?)*?(asdf)?",
        "((asdf)?$)*?",
        "((asdf)?)*?((asdf)?$)*?",
        
        "(a?)+a{10}",
        "(a?)+?a{10}",
        "(a?)*a{10}",
        "(a?)*?a{10}",
        "()",
        "(a|)*b",
        "(z?)*a{10}",
        
        
        // possessive
        "(b|a|)*+",
        "(a|)*+b",
        "(?>(b|a|)*)",
        "(b|a|)*+b",
        "(b|a|as|q)*+",
        "(b|a|as|q)*+X",
        "(b|a|as|q)*",
        "(b|a|as|q)*X",
        "a++ab",
        
        "[0-9]+\\.[0-9]+",
        "[0-9]+0\\.[0-9]+",
        
        "(a|a|ab)bc",
        "(ab|ab|a)bc",
        "[0-9]\\.[0-9]",
        
        "\\d\\.\\d",
        "\\d*\\.\\d*",
        "\\w+",
        "\\s+",
        "\\s(\\w+)",
        "\\w+\\s",
        
        "(\\d)*?\\.(\\d)+",
        "([0-9])*?\\.([0-9])+",
        "([0-9]){3,5}?\\.([0-9])+",
        "[0-9]{3,5}?\\.[0-9]+",
        "([0-9]){3,5}\\.([0-9])+",
        "[0-9]{3,5}\\.[0-9]+",
        "(a|ab)*b",
        "(ab?)*?b",
        "(ab?\?)*b",
        "(a)?\?(b|a)",
        "(a)*a{10}",
        "(a)*?a{10}",
        "a()a",
        "a(|)a",
        "a(|){1}?a",
        "a(|b)+a",
        "a(|b)+?a",
        "(a|b)*?b",
        "a*a*?",
        "a*?a*",
        "(b|a)*b",
        "(b|a)*?b",
        "(b|a|)*",
        "(b|a|)*bb",
        "(b|a|)*?bb",
        "(|a)+",
        "(|a)+?",
        "()+",
        "()+?",
        "(|)+?",
        "a(|)*a",
        "a(|)*?a",
        "(a|(((()))))*b",
        "((\\w+,?)*:)*",
        "((\\w+,?)*+:)*",
        "((\\w+,?)*+:)*+",
        
        // pathological
        "((a?b|a)b?)*",
        "(.*,){11}P",
        "(.*?,){11}P",
    };
    static const char * texts[] = {
        "testacc@example.com",
        
        "aa.bb.cc.dd",
        "a5,b7,c9",
        "a5,b7,c9,",
        "a5,b7,c9,,",
        "a5,b7,c9,1",
        "a5,b7,c9,a",
        "",
        " ",
        "  ",
        "a",
        "aa",
        "aba) ",
        "aaaaaaaaa",
        "aaaaaaaaaa",
        "aaaaaaaaaaaaaa",
        "aaaaaaaaaaaaaab",
        "aaaaaaaaaaaaaaba",
        
        "testacc@example.com",
        "test+acc@example.com",
        "test.acc@example.com",
        "test.acc.acc@sub.example.com",
        "test.acc@sub.example.com",
        "test@sub.example.com",
        "@example.com",
        "example.com",
        "a@",
        "#@%^%#$@#$@#.com",
        "Joe Smith <email@example.com>",
        "_______@example.com",
        "“email”@example.com",
        "email@[123.123.123.123]",
        "email@123.123.123.123",
        
        "abc) ",
        "abba) ",
        "abbc) ",
        "012.53) ",
        ".53) ",
        "5.5",
        "022134.53) ",
        "02234.53) ",
        "1131.53) ",
        "131.53) ",
        "11.53) ",
        "1.53) ",
        "aa",
        "aaaaaaaaabababab",
        "aaaaaaaaababababb",
        "aaaaabbbbbbbx",
        "bbbbbbb",
        "asqbX",
        "1,2,3,4,5,6,7,8,9,10,11,12",
        "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16",
        
        "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34",
        //"1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35",
        "aaaaaababababababaabx",
        
        "       ",
        "afd1gkage919953bd       ",
        "   x    ",
        "   ,\\1264ga0b a    ",
        "asdf ",
        "asdfg",
        "   asdf",
        "asdf   ",
        "   asdf   ",
        "XXXasdf",
        "asdfXXX",
        "XXXasdfXXX",
        "000asdf",
        "asdf000",
        "000asdf000",
        "a,b,easbe_1:a,:a",
    };
    
    
    for (size_t i = 0; i < sizeof(regexes) / sizeof(regexes[0]); i++)
    //for (size_t i = 0; 0; i++)
    {
        const char * regex = regexes[i];
        
        RegexToken tokens[256];
        memset(tokens, 0xFF, sizeof(tokens));
        int16_t token_count = sizeof(tokens)/sizeof(tokens[0]);
        int e = regex_parse(regex, tokens, &token_count, 0);
        assert(!e);
        
        bool has_possessive = false;
        for (int32_t n = 0; n < token_count; n++)
        {
            if (tokens[n].mode & RXTOK_MODE_POSSESSIVE)
            {
                has_possessive = true;
                break;
            }
        }
        
        std::string regex_str = regex;
        
        std::regex cppregex;
        if (!has_possessive)
            cppregex = std::regex{regex_str, std::regex_constants::ECMAScript};
        
        print_regex_tokens(tokens);
        
        for (size_t j = 0; j < sizeof(texts) / sizeof(texts[0]); j++)
        {
            const char * text = texts[j];
            std::string text_str = text;
            
            std::cmatch cm;
            using clock = std::chrono::high_resolution_clock;
            
            int64_t std_len = -1;
            
            if (!has_possessive)
            {
                printf("testing std c++ regex `%s` on string `%s`...\n", regex, text);
                fflush(stdout);
                
                auto start = clock::now();
                bool s = std::regex_search(text, cm, cppregex);
                double t = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() / 1000000.0;
                if (s)
                {
                    const char * where = cm[0].first;
                    size_t offs = where - text;
                    if (offs == 0)
                        std_len = cm[0].second - where;
                    printf("c++ regex found match at %zd with len %zd after %f seconds\n", offs, std_len, t);
                }
                else
                    printf("c++ regex found no match after %f seconds\n", t);
            }
            else
                puts("skipping c++ because it has a possessive");
            
            printf("testing my regex `%s` on string `%s`...\n", regex, text);
            
            auto start = clock::now();
            
            int64_t cap_pos[16];
            int64_t cap_span[16];
            memset(cap_pos, 0xFF, sizeof(cap_pos));
            memset(cap_span, 0xFF, sizeof(cap_span));
            
            int64_t match_len = regex_match(tokens, text, 16, cap_pos, cap_span);
            double t = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() / 1000000.0;
            
            assert(match_len != -3);
            if (match_len >= 0)
                printf("my regex found match with len %zd after %f seconds\n", match_len, t);
            else if (match_len == -2)
                printf("my regex ran out of memory after %f seconds (note: `%s`)\n", t, regex);
            else
                printf("my regex found no match after %f seconds\n", t);
            
            // ignore special cases
            if (regex_str != "(|a)+" &&
                regex_str != "(|a)+?" &&
                !has_possessive)
            {
                printf("comparing %zd to %zd...\n", match_len, std_len);
                printf("regex `%s`, string `%s`\n", regex, text);
                assert(match_len == std_len);
                puts("comparing captures...");
                if (match_len >= 0)
                {
                    for (size_t x = 0; x < 16; x++)
                    {
                        //size
                    }
                }
            }
        }
    }
    
    RegexToken tokens[256];
    int16_t token_count = sizeof(tokens)/sizeof(tokens[0]);
    //int e = regex_parse("((\\w+,?)*:)*", tokens, &token_count, 0);
    //int e = regex_parse("((\\w+,?)*+:)*", tokens, &token_count, 0);
    //int e = regex_parse("((\\w+,?)*+:)*+", tokens, &token_count, 0);
    int e = regex_parse("((a)|(b))++", tokens, &token_count, 0);
    //int e = regex_parse("((\\w+,?)*:)", tokens, &token_count, 0);
    //int e = regex_parse("((a)|((b)q))*", tokens, &token_count, 0);
    assert(!e);
    
    int64_t cap_pos[5];
    int64_t cap_span[5];
    memset(cap_pos, 0xFF, sizeof(cap_pos));
    memset(cap_span, 0xFF, sizeof(cap_span));
    //int64_t matchlen = regex_match(tokens, "a,b,easbe_1:aaa,_,:a", 5, cap_pos, cap_span);
    //int64_t matchlen = regex_match(tokens, "aabqaaaaba", 5, cap_pos, cap_span);
    int64_t matchlen = regex_match(tokens, "aaaaaabbbabaqa", 5, cap_pos, cap_span);
    printf("Match length: %zd\n", matchlen);
    for (int i = 0; i < 5; i++)
        printf("Capture %d: %zd plus %zd\n", i, cap_pos[i], cap_span[i]);
    
    print_regex_tokens(tokens);
    
    puts("All regex tests passed!");
}

int main(void)
{
    testify();
}
