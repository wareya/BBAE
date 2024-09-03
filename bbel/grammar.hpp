#ifndef BBEL_GRAMMAR
#define BBEL_GRAMMAR

#include <cassert>
#include <cstring>
#include <cstdint>

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>

#include "re.c"

// safe wrappers around re.c
struct Regex {
    regex_info info;
    Regex(regex_info r) : info(r) {}
    ~Regex()
    {
        free(info.re_compiled);
        free(info.ccl_buf);
    }
};

std::shared_ptr<Regex> regex_compile(const char* pattern)
{
    auto r = re_compile(pattern);
    auto ret = std::make_shared<Regex>(r);
    return ret;
}

int regex_match(std::shared_ptr<Regex> pattern, const char * text, int * matchlength)
{
    return re_matchp(pattern->info.re_compiled, text, matchlength);
}

enum MatchKind {
    MATCH_KIND_INVALID,
    MATCH_KIND_LITERAL,
    MATCH_KIND_REGEX,
    MATCH_KIND_POINT,
};
enum MatchQualifier {
    MATCH_QUAL_DEFAULT,
    MATCH_QUAL_STAR,
    MATCH_QUAL_PLUS,
    MATCH_QUAL_MAYBE,
};

struct GrammarPoint;
struct MatchingRule {
    MatchKind kind = MATCH_KIND_INVALID;
    std::shared_ptr<std::string> text = 0;
    std::shared_ptr<Regex> compiled_regex = 0;
    std::shared_ptr<GrammarPoint> rule = 0;
    MatchQualifier qualifier;
    bool lazy = false;
};

struct GrammarForm {
    std::vector<MatchingRule> rules;
};

struct GrammarPoint {
    std::shared_ptr<std::string> name = 0;
    std::vector<GrammarForm> forms;
    GrammarPoint()
    {
        forms.push_back(GrammarForm{});
    }
};

MatchingRule new_rule_regex(std::string regex)
{
    return MatchingRule { MATCH_KIND_REGEX, std::make_shared<std::string>(regex), 0, 0, MATCH_QUAL_DEFAULT, false };
}
MatchingRule new_rule_name(std::string name)
{
    return MatchingRule { MATCH_KIND_POINT, std::make_shared<std::string>(name), 0, 0, MATCH_QUAL_DEFAULT, false };
}
MatchingRule new_rule_point(GrammarPoint point)
{
    return MatchingRule { MATCH_KIND_POINT, 0, 0, std::make_shared<GrammarPoint>(point), MATCH_QUAL_DEFAULT, false };
}
MatchingRule new_rule_text(std::string text)
{
    return MatchingRule { MATCH_KIND_LITERAL, std::make_shared<std::string>(text), 0, 0, MATCH_QUAL_DEFAULT, false };
}

struct Grammar
{
    std::unordered_map<std::string, std::shared_ptr<GrammarPoint>> points;
    std::vector<MatchingRule> tokens;
};

    
bool starts_with(const char * str, const char * prefix)
{
    for (size_t i = 0; prefix[i] != 0; i++)
    {
        if (str[i] != prefix[i])
            return false;
    }
    return true;
};

auto load_grammar(const char * text) -> Grammar
{
    assert(text);
    
    std::unordered_map<std::string, std::shared_ptr<GrammarPoint>> ret = {};
    std::vector<MatchingRule> tokens;
    
    enum Mode {
        MODE_NAME,
        MODE_FORMS,
    };
    
    Mode mode = MODE_NAME;
    
    size_t len = strlen(text);
    size_t i = 0;
    
    auto line_is_at_eol = [&]()
    {
        return text[i] == 0 || text[i] == '\n' || text[i] == '\r';
    };
    auto line_is_at_space = [&]()
    {
        return text[i] == ' ';
    };
    auto line_is_empty = [&]()
    {
        if (line_is_at_eol())
            return true;
        while (!line_is_at_eol() && line_is_at_space())
            i++;
        return line_is_at_eol();
    };
    auto go_to_next_line = [&]()
    {
        while (!line_is_at_eol())
            i++;
        while (line_is_at_eol() && text[i] != 0)
            i++;
    };
    
    auto is_start_of_name = [](char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    
    auto is_part_of_name = [&](char c)
    {
        return is_start_of_name(c) || (c >= '0' && c <= '9');
    };
    
    GrammarPoint current_point{};
    
    while (i < len)
    {
        while (line_is_empty() && text[i] != 0)
        {
            if (current_point.name)
            {
                ret.insert({*current_point.name, std::make_shared<GrammarPoint>(current_point)});
                current_point = {};
            }
            mode = MODE_NAME;
            go_to_next_line();
        }
        //size_t n = i;
        if (mode == MODE_NAME)
        {
            while (line_is_at_space())
                i++;
            std::string name;
            //assert();
            while (text[i] != 0 && text[i] != ':')
            {
                name += text[i];
                i++;
            }
            current_point.name = std::make_shared<std::string>(name);
            mode = MODE_FORMS;
            
            go_to_next_line();
        }
        else if (mode == MODE_FORMS)
        {
            //printf("form in... %s\n", current_point.name->data());
            std::vector<GrammarPoint> point_stack;
            
            while (!line_is_empty())
            {
                auto & form = current_point.forms.back();
                
                while (line_is_at_space())
                    i++;
                
                //printf("starting at... %zd\n", i - n);
                if (starts_with(&text[i], "rx%"))
                {
                    i += 3;
                    std::string regex_text = "";
                    while (text[i] != 0 && !starts_with(&text[i], "%rx"))
                    {
                        regex_text += text[i];
                        i++;
                    }
                    assert(((void)"missing terminator for regex token", starts_with(&text[i], "%rx")));
                    i += 3;
                    
                    auto rule = new_rule_regex(regex_text);
                    tokens.push_back(rule);
                    form.rules.push_back(rule);
                    
                    //printf("regex: %s\n", regex_text.data());
                }
                else if (is_start_of_name(text[i]))
                {
                    std::string name = "";
                    while (is_part_of_name(text[i]))
                    {
                        name += text[i];
                        i++;
                    }
                    auto rule = new_rule_name(name);
                    form.rules.push_back(rule);
                    //printf("name: %s\n", name.data());
                }
                else if (text[i] == '"')
                {
                    //puts("starting string...");
                    i++;
                    std::string nutext = "";
                    bool in_escape = false;
                    while (1)
                    {
                        if (in_escape && (text[i] == '\\' || text[i] == '"'))
                        {
                            nutext += text[i];
                            in_escape = false;
                            i += 1;
                        }
                        else if (in_escape)
                        {
                            assert(((void)"error: unknown escape sequence!", 0));
                        }
                        else if (text[i] == '"')
                        {
                            i += 1;
                            break;
                        }
                        else if (text[i] == '\\')
                        {
                            i += 1;
                            in_escape = true;
                        }
                        else if (line_is_at_eol())
                        {
                            assert(((void)"error: unterminated string!", 0));
                        }
                        else
                        {
                            nutext += text[i];
                            i += 1;
                        }
                    }
                    auto rule = new_rule_text(nutext);
                    tokens.push_back(rule);
                    form.rules.push_back(rule);
                    //printf("string: %s\n", nutext.data());
                }
                else if (text[i] == '(')
                {
                    point_stack.push_back(current_point);
                    current_point = {};
                    i += 1;
                }
                else if (text[i] == ')')
                {
                    auto rule = new_rule_point(current_point);
                    current_point = point_stack.back();
                    point_stack.pop_back();
                    current_point.forms.back().rules.push_back(rule);
                    i += 1;
                }
                else if (text[i] == '|')
                {
                    current_point.forms.push_back(GrammarForm{});
                    i += 1;
                }
                else if (text[i] == '*')
                {
                    assert(form.rules.size() > 0);
                    assert(form.rules.back().qualifier == MATCH_QUAL_DEFAULT);
                    form.rules.back().qualifier = MATCH_QUAL_STAR;
                    i += 1;
                }
                else if (text[i] == '+')
                {
                    assert(form.rules.size() > 0);
                    assert(form.rules.back().qualifier == MATCH_QUAL_DEFAULT);
                    form.rules.back().qualifier = MATCH_QUAL_PLUS;
                    i += 1;
                }
                else if (text[i] == '?' && form.rules.back().qualifier == MATCH_QUAL_DEFAULT)
                {
                    assert(form.rules.size() > 0);
                    assert(form.rules.back().qualifier == MATCH_QUAL_DEFAULT);
                    form.rules.back().qualifier = MATCH_QUAL_MAYBE;
                    i += 1;
                }
                else if (text[i] == '?' && form.rules.back().qualifier != MATCH_QUAL_DEFAULT)
                {
                    assert(form.rules.size() > 0);
                    assert(!form.rules.back().lazy);
                    form.rules.back().lazy = true;
                    i += 1;
                }
                else
                {
                    //printf("TODO: %c\n", text[i]);
                    assert(0);
                }
            }
            i += 1;
            assert(((void)"unterminated parenthesis", point_stack.size() == 0));
            current_point.forms.push_back(GrammarForm{});
        }
        else
            assert(0);
    }
    
    if (current_point.name)
    {
        ret.insert({*current_point.name, std::make_shared<GrammarPoint>(current_point)});
        current_point = {};
    }
    
    std::sort(tokens.begin(), tokens.end(), [](MatchingRule a, MatchingRule b)
    {
        if (a.kind == MATCH_KIND_REGEX && b.kind != MATCH_KIND_REGEX)
            return 1;
        if (a.kind != MATCH_KIND_REGEX && b.kind == MATCH_KIND_REGEX)
            return 0;
        if (a.text->length() > b.text->length())
            return 1;
        if (a.text->length() < b.text->length())
            return 0;
        if (strcmp(a.text->data(), b.text->data()) < 0)
            return 0;
        else
            return 1;
    });
    
    for (size_t i = tokens.size() - 1; i > 0; i--)
    {
        auto a = tokens[i - 1];
        auto b = tokens[i];
        if (a.kind == b.kind && *a.text == *b.text)
            tokens.erase(tokens.begin() + i);
    }
    
    for (auto & n : tokens)
    {
        if (n.kind == MATCH_KIND_REGEX)
        {
            auto s = std::string("^") + *n.text;
            //auto s = *n.text;
            //printf("%s\n", s.data());
            n.compiled_regex = regex_compile(s.data());
            assert(&*n.compiled_regex);
        }
        
        //printf("%s\n", n.text->data());
    }
    
    /*
    for (const auto & [name, point] : ret)
    {
        printf("rule `%s` exists\n", name.data());
    }
    */
    for (auto & [name, point] : ret)
    {
        for (auto & form : point->forms)
        {
            for (auto & rule : form.rules)
            {
                if (rule.kind == MATCH_KIND_POINT && rule.text != 0)
                {
                    if (ret.count(*rule.text) == 0)
                    {
                        printf("Attempted to use grammar rule with name `%s`, which is not defined\n", rule.text->data());
                        assert(0);
                    }
                    rule.rule = ret[*rule.text];
                    rule.text = 0;
                }
            }
        }
    }
    
    return {ret, tokens};
}

struct ASTNode
{
    std::vector<std::shared_ptr<ASTNode>> children;
};

struct Token {
    std::string text = "";
    size_t index = 0;
    size_t row = 0;
    size_t column = 0;
};

std::vector<Token> tokenize(const Grammar & grammar, const char * _text)
{
    const std::vector<MatchingRule> & tokens = grammar.tokens;
    std::vector<Token> ret;
    
    std::string text = _text;
    
    size_t i = 0;
    size_t text_len = text.size();
    
    size_t row = 1;
    size_t column = 1;
    
    while (1)
    {
        if (i >= text_len)
        {
            puts("end of string, breaking");
            break;
        }
        // ignore whitespace between tokens
        auto skip_whitespace = [&]()
        {
            while (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')
            {
                if (text[i] == '\n')
                {
                    row += 1;
                    column = 1;
                }
                else
                    column += 1;
                i++;
            }
        };
        auto skip_comment = [&]()
        {
            if (starts_with(&text[i], "//"))
            {
                while (text[i] != '\n' && text[i] != 0)
                    i++;
            }
            else if (starts_with(&text[i], "/*"))
            {
                i += 2;
                while (text[i] != 0 && !starts_with(&text[i], "*/"))
                    i++;
            }
        };
        auto is_whitespace_or_comment = [&]()
        {
            return text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n'
                || starts_with(&text[i], "//") || starts_with(&text[i], "/*");
        };
        
        while (is_whitespace_or_comment())
        {
            skip_whitespace();
            skip_comment();
        }
        
        if (text[i] == 0)
        {
            //puts("null. breaking");
            break;
        }
        
        bool found = false;
        for (auto token : tokens)
        {
            if (token.kind == MATCH_KIND_REGEX)
            {
                int len = 0;
                //printf("%s\n", token.text->data());
                //printf("%p\n", &*token.compiled_regex);
                
                //printf("trying to match %s at...%s\n", token.text->data(), &text[i]);
                int index = regex_match(token.compiled_regex, &text[i], &len);
                assert(index == 0 || index == -1);
                //printf("%d %d\n", index, len);
                if (index == 0)
                {
                    assert(len > 0);
                    std::string mystr = text.substr(i, len);
                    ret.push_back(Token{mystr, i, row, column});
                    
                    found = true;
                    column += len;
                    i += len;
                    break;
                }
            }
            else if (token.kind == MATCH_KIND_LITERAL)
            {
                if (starts_with(&text[i], token.text->data()))
                {
                    std::string mystr = text.substr(i, token.text->size());
                    ret.push_back(Token{mystr, i, row, column});
                    
                    found = true;
                    column += token.text->size();
                    i += token.text->size();
                    break;
                }
            }
            else
                assert(((void)"Broken internal grammar state!!!!!!", 0));
        }
        if (!found)
        {
            printf("failed to tokenize! got to line %zd column %zd (index %zd)\n", row, column, i);
            return ret;
        }
        assert(found);
    }
    
    return ret;
}

/*
auto parse(Grammar & grammar, const char * text) -> std::shared_ptr<ASTNode>
{
    
}
*/

#endif // BBEL_GRAMMAR
