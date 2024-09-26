
// testing requires PCRE2
// msys2: pacman -S mingw-w64-<flavor>-pcre2
// linker flag is usually -lpcre2-8

//#define REGEX_VERBOSE
#include "my_regex.h"

#include <chrono>
#include <string>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

void testify(void)
{
    static const char * regexes[] = {
        "",
        "(|b|a|as|q)*X",
        "(b|a|as|q|)*X",
        "(b|a|as|q|)*?X",
        "((b|a|as|q|))*X",
        "((b|a|as|q|))*?X",
        "(b|a|as|q)*X",
        "(b|a|as|q)*?X",
        "(b|a|as|q)+X",
        "(b|a|as|q|)+X",
        
        "(|a?)+?a{10}",
        "(a?)+?a{10}",
        "(a?)+a{10}",
        "(a)+a{9}",
        "(a)+?a{9}",
        "(|a)+a{9}",
        "(|a)+a{10}",
        "(|a)+a{11}",
        "(|a)+?a{11}",
        "^a(bc+|b[eh])g|.h$",
        "(bc+d$|ef*g.|h?i(j|k))",
        
        "(b|a|as|q)*?X",
        // (?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\])
        "(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|\"(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21\\x23-\\x5b\\x5d-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])*\")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21-\\x5a\\x53-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])+)\\])",
        //"(?:\\w+(?:\\.\\w+)*)@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)",
        //"(?:\\w+(?:\\.\\w+)*)@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)",
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
        
        "mistaken bogus regex",
    };
    static const char * texts[] = {
        "aaaaaaaaaa",
        "asqbX",
        "asqb",
        "abh",
        
        "effgz",
        "ij",
        "effg",
        "bcdd",
        "reffgz",
        
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
        
        "uh-uh",
        "words, yeah",
        "mistaken bogus regex",
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
        
        printf("token count: %d\n", token_count);
        print_regex_tokens(tokens);
        
        std::string regex_str = regex;
        
        int errorcode;
        PCRE2_SIZE erroroffset;
        pcre2_code * re = pcre2_compile(PCRE2_SPTR8(regex), PCRE2_ZERO_TERMINATED, PCRE2_ANCHORED | PCRE2_NO_UTF_CHECK,
                                        &errorcode, &erroroffset, NULL);
        
        for (size_t j = 0; j < sizeof(texts) / sizeof(texts[0]); j++)
        {
            const char * text = texts[j];
            std::string text_str = text;
            
            using clock = std::chrono::high_resolution_clock;
            
            int64_t pcre2_len = -1;
            
            printf("testing PCRE2 regex `%s` on string `%s`...\n", regex, text);
            fflush(stdout);
            
            auto start = clock::now();
            pcre2_match_data * match_data = pcre2_match_data_create_from_pattern(re, 0);
            int submatch_count = pcre2_match(re, PCRE2_SPTR8(text), text_str.size(), 0, PCRE2_ANCHORED | PCRE2_NO_UTF_CHECK, match_data, 0); 
            double t = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() / 1000000.0;
            
            //printf("submatch count: %d\n", submatch_count);
            //printf("ovector count: %d\n", pcre2_get_ovector_count(match_data));
            
            PCRE2_SIZE * ovector;
            if (submatch_count > 0)
            {
                ovector = pcre2_get_ovector_pointer(match_data);
                size_t offs = ovector[0];
                if (offs == 0)
                    pcre2_len = ovector[1] - offs;
                printf("pcre2 regex found match at %zd with len %zd after %f seconds\n", offs, pcre2_len, t);
            }
            else
                printf("pcre2 regex found no match after %f seconds\n", t);
            
            printf("testing my regex `%s` on string `%s`...\n", regex, text);
            
            start = clock::now();
            
            int64_t cap_pos[16];
            int64_t cap_span[16];
            memset(cap_pos, 0xFF, sizeof(cap_pos));
            memset(cap_span, 0xFF, sizeof(cap_span));
            
            int64_t match_len = regex_match(tokens, text, 0, 16, cap_pos, cap_span);
            t = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() / 1000000.0;
            
            assert(match_len != -3);
            if (match_len >= 0)
                printf("my regex found match with len %zd after %f seconds\n", match_len, t);
            else if (match_len == -2)
                printf("my regex ran out of memory after %f seconds (note: `%s`)\n", t, regex);
            else
                printf("my regex found no match after %f seconds\n", t);
            
            // we define captures differently than PCRE2 for possessives, so skip them
            if (!has_possessive && submatch_count > 0)
            {
                printf("comparing %zd to %zd...\n", match_len, pcre2_len);
                printf("regex `%s`, string `%s`\n", regex, text);
                assert(match_len == pcre2_len);
                puts("comparing captures...");
                if (match_len >= 0)
                {
                    for (int x = 0; x < submatch_count && x < 16; x++)
                    {
                        size_t where = ovector[x*2];
                        if (where == 0)
                        {
                            size_t pcre2_len = ovector[x*2+1] - where;
                            // probably a situation of std capturing a zero-length group repetition
                            printf("Capture %d: std (%zd,%zd)  mine (%zd,%zd)\n", x, where, pcre2_len, cap_pos[x], cap_span[x]);
                            if (!(cap_pos[x] == -1 && cap_span[x] == -1 && where == 0 && pcre2_len == 0))
                            {
                                assert(where     == (size_t)cap_pos[x]);
                                assert(pcre2_len == (size_t)cap_span[x]);
                            }
                        }
                    }
                }
            }
            
            pcre2_match_data_free(match_data);
        }
        pcre2_code_free(re);
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
    int64_t matchlen = regex_match(tokens, "aaaaaabbbabaqa", 0, 5, cap_pos, cap_span);
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
