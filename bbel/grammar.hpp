#ifndef BBEL_GRAMMAR
#define BBEL_GRAMMAR

#include <cassert>
#include <cstring>
#include <cstdint>

#include <utility>

#include "types.hpp"

#include "my_regex/my_regex.h"

struct Regex {
    RegexToken tokens[64];
    String str;
    int16_t token_count;
    Regex(String s) : str(s), token_count(0)
    {
        token_count = 64;
        int e = regex_parse(str.data(), tokens, &token_count, 0);
        assert(!e);
    }
    inline int match(const char * text, int * matchlength)
    {
        int ret = regex_match(tokens, text, 0, 0, 0, 0);
        if (ret > 0)
        {
            *matchlength = ret;
            return 0;
        }
        return -1;
    }
};

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
    Shared<String> text = 0;
    Shared<Regex> compiled_regex = 0;
    Shared<GrammarPoint> rule = 0;
    // qualified matches are possessive-greedy by default
    MatchQualifier qualifier = MATCH_QUAL_DEFAULT;
    bool lazy = false;
    bool greedy = false;
};

struct GrammarForm {
    Vec<Shared<MatchingRule>> rules;
};

struct GrammarPoint {
    Shared<String> name = 0;
    Vec<GrammarForm> forms;
    bool left_recursive = false;
    bool no_tokens = false;
    GrammarPoint()
    {
        forms.push_back(GrammarForm{});
    }
};

static Shared<MatchingRule> new_rule_regex(String regex)
{
    auto ret = MatchingRule { MATCH_KIND_REGEX, MakeShared<String>(regex), 0, 0 };
    return MakeShared<MatchingRule>(std::move(ret));
}
static Shared<MatchingRule> new_rule_name(String name)
{
    auto ret = MatchingRule { MATCH_KIND_POINT, MakeShared<String>(name), 0, 0 };
    return MakeShared<MatchingRule>(std::move(ret));
}
static Shared<MatchingRule> new_rule_point(Shared<GrammarPoint> point)
{
    auto ret = MatchingRule { MATCH_KIND_POINT, 0, 0, point };
    return MakeShared<MatchingRule>(std::move(ret));
}
static Shared<MatchingRule> new_rule_text(String text)
{
    auto ret = MatchingRule { MATCH_KIND_LITERAL, MakeShared<String>(text), 0, 0 };
    return MakeShared<MatchingRule>(std::move(ret));
}

struct Grammar
{
    ListMap<String, Shared<GrammarPoint>> points;
    Vec<Shared<MatchingRule>> tokens;
    Vec<Shared<MatchingRule>> regex_tokens;
    ~Grammar()
    {
        // kill inter-point references to prevent reference cycle memory leaks
        for (auto & [_, point] : points)
        {
            for (auto & form : point->forms)
            {
                for (auto & rule : form.rules)
                {
                    rule->rule = 0;
                }
            }
        }
    }
};

void debug_print_grammar_points(Grammar & grammar)
{
    for (const auto & [name, point] : grammar.points)
    {
        printf("- grammar point name: %s\n", name.data());
        for (const auto & form : point->forms)
        {
            printf("- form: ");
            for (const auto & rule : form.rules)
            {
                if (rule->text)
                    printf(" %s", rule->text->data());
                else
                    printf(" (anon)");
            }
            printf("\n");
        }
    }
}

    
static bool starts_with(const char * str, const char * prefix)
{
    for (size_t i = 0; prefix[i] != 0; i++)
    {
        if (str[i] != prefix[i])
            return false;
    }
    return true;
};

static auto load_grammar(const char * text) -> Grammar
{
    assert(text);
    
    ListMap<String, Shared<GrammarPoint>> ret;
    ListSet<Shared<GrammarPoint>> all_points;
    Vec<Shared<MatchingRule>> tokens;
    Vec<Shared<MatchingRule>> regex_tokens;
    
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
    
    auto current_point = MakeShared<GrammarPoint>();
    
    while (i < len)
    {
        while (line_is_empty() && text[i] != 0)
        {
            if (current_point->name)
            {
                ret.insert(*current_point->name, current_point);
                all_points.insert(current_point);
                current_point = MakeShared<GrammarPoint>();
            }
            mode = MODE_NAME;
            go_to_next_line();
        }
        //size_t n = i;
        if (mode == MODE_NAME)
        {
            while (line_is_at_space())
                i++;
            String name;
            //assert();
            while (text[i] != 0 && text[i] != ':')
            {
                name += text[i];
                i++;
            }
            i++; // ':'
            while (text[i] != 0 && (text[i] == ' ' || text[i] == '\t'))
                i++;
            if (starts_with(&text[i], "@left_recursive"))
                current_point->left_recursive = true;
            if (starts_with(&text[i], "@notokens"))
                current_point->no_tokens = true;
            
            current_point->name = MakeShared<String>(name);
            mode = MODE_FORMS;
            
            go_to_next_line();
        }
        else if (mode == MODE_FORMS)
        {
            //printf("form in... %s\n", current_point->name->data());
            Vec<Shared<GrammarPoint>> point_stack;
            
            while (!line_is_empty())
            {
                auto & form = current_point->forms.back();
                
                while (line_is_at_space())
                    i++;
                
                //printf("starting at... %zd\n", i - n);
                if (starts_with(&text[i], "rx%"))
                {
                    i += 3;
                    String regex_text = "";
                    while (text[i] != 0 && !starts_with(&text[i], "%rx"))
                    {
                        regex_text += text[i];
                        i++;
                    }
                    assert(((void)"missing terminator for regex token", starts_with(&text[i], "%rx")));
                    i += 3;
                    
                    auto rule = new_rule_regex(regex_text);
                    tokens.push_back(rule);
                    regex_tokens.push_back(rule);
                    form.rules.push_back(rule);
                    
                    //printf("regex: %s\n", regex_text.data());
                }
                else if (is_start_of_name(text[i]))
                {
                    String name = "";
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
                    String nutext = "";
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
                            in_escape = true;
                            i += 1;
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
                    current_point = MakeShared<GrammarPoint>();
                    i += 1;
                }
                else if (text[i] == ')')
                {
                    current_point->forms.push_back(GrammarForm{}); // dummy to match full-fledged forms in having a dummy at the end
                    auto rule = new_rule_point(current_point);
                    all_points.insert(current_point);
                    current_point = point_stack.back();
                    point_stack.pop_back();
                    current_point->forms.back().rules.push_back(rule);
                    i += 1;
                }
                else if (text[i] == '|')
                {
                    current_point->forms.push_back(GrammarForm{});
                    i += 1;
                }
                else if (text[i] == '+')
                {
                    assert(form.rules.size() > 0);
                    assert(form.rules.back()->qualifier == MATCH_QUAL_DEFAULT);
                    form.rules.back()->qualifier = MATCH_QUAL_PLUS;
                    i += 1;
                }
                else if (text[i] == '*')
                {
                    assert(form.rules.size() > 0);
                    if (form.rules.back()->qualifier == MATCH_QUAL_DEFAULT)
                    {
                        assert(form.rules.back()->qualifier == MATCH_QUAL_DEFAULT);
                        form.rules.back()->qualifier = MATCH_QUAL_STAR;
                    }
                    else
                    {
                        assert(!form.rules.back()->lazy);
                        assert(!form.rules.back()->greedy);
                        form.rules.back()->greedy = true;
                    }
                    i += 1;
                }
                else if (text[i] == '?')
                {
                    assert(form.rules.size() > 0);
                    if (form.rules.back()->qualifier == MATCH_QUAL_DEFAULT)
                    {
                        assert(form.rules.back()->qualifier == MATCH_QUAL_DEFAULT);
                        form.rules.back()->qualifier = MATCH_QUAL_MAYBE;
                    }
                    else
                    {
                        assert(!form.rules.back()->lazy);
                        assert(!form.rules.back()->greedy);
                        form.rules.back()->lazy = true;
                    }
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
            current_point->forms.push_back(GrammarForm{});
        }
        else
            assert(0);
    }
    
    if (current_point->name)
    {
        ret.insert(*current_point->name, current_point);
        all_points.insert(current_point);
    }
    
    //std::sort(tokens.begin(), tokens.end(), [](Shared<MatchingRule> a, Shared<MatchingRule> b)
    tokens.sort([](Shared<MatchingRule> a, Shared<MatchingRule> b)
    {
        if (a->kind == MATCH_KIND_REGEX && b->kind != MATCH_KIND_REGEX)
            return 1;
        if (a->kind != MATCH_KIND_REGEX && b->kind == MATCH_KIND_REGEX)
            return 0;
        if (a->text->length() > b->text->length())
            return 1;
        if (a->text->length() < b->text->length())
            return 0;
        if (strcmp(a->text->data(), b->text->data()) < 0)
            return 0;
        else
            return 1;
    });
    
    for (size_t i = tokens.size() - 1; i > 0; i--)
    {
        auto a = tokens[i - 1];
        auto b = tokens[i];
        if (a->kind == b->kind && *a->text == *b->text)
            tokens.erase(tokens.begin() + i);
    }
    
    /*
    for (const auto & [name, point] : ret)
    {
        printf("rule `%s` exists\n", name.data());
    }
    */
    for (auto & point : all_points)
    {
        assert(point->forms.size() > 0);
        assert(point->forms.back().rules.size() == 0);
        point->forms.pop_back(); // remove dummy
        
        for (auto & form : point->forms)
        {
            for (auto & rule : form.rules)
            {
                assert(rule->kind != MATCH_KIND_INVALID);
                
                if (rule->kind == MATCH_KIND_REGEX)
                {
                    assert(rule->text);
                    auto s = String("^") + *rule->text;
                    rule->compiled_regex = MakeShared<Regex>(s.data());
                    assert(&*rule->compiled_regex);
                }
                if (rule->kind == MATCH_KIND_POINT && rule->text != 0)
                {
                    if (ret.count(*rule->text) == 0)
                    {
                        printf("Attempted to use grammar rule with name `%s`, which is not defined\n", rule->text->data());
                        assert(0);
                    }
                    rule->rule = ret[*rule->text];
                    //rule->text = 0;
                }
            }
        }
    }
    
    return {ret, tokens, regex_tokens};
}

struct Token {
    Shared<String> text = 0;
    Shared<Regex> from_regex = 0;
    size_t index = 0;
    size_t line_index = 0;
    size_t row = 0;
    size_t column = 0;
    bool valid = true;
};

// On success, the token stream is returned.
// On failure, the tokenization process is returned, with an additional token with null text and regex at the end.
static Vec<Shared<Token>> tokenize(const Grammar & grammar, const char * _text)
{
    const Vec<Shared<MatchingRule>> & tokens = grammar.tokens;
    Vec<Shared<Token>> ret;
    
    String text = _text;
    
    size_t i = 0;
    size_t text_len = text.size();
    
    size_t row = 1;
    size_t column = 1;
    size_t line_index = 0;
    
    while (1)
    {
        if (i >= text_len)
        {
            //puts("end of string, breaking");
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
                    line_index = i + 1;
                }
                else if (text[i] == '\t')
                {
                    column += 1;
                    while ((column - 1) % 4)
                        column += 1;
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
                line_index = i + 1;
            }
            else if (starts_with(&text[i], "/*"))
            {
                i += 2;
                while (text[i] != 0 && !starts_with(&text[i], "*/"))
                {
                    if (text[i] == '\n')
                        line_index = i + 1;
                    i++;
                }
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
        
        size_t longest_found = 0;
        Shared<MatchingRule> found;
        for (auto _token : tokens)
        {
            auto & token = *_token;
            if (token.kind == MATCH_KIND_REGEX)
            {
                int len = 0;
                int index = token.compiled_regex->match(&text[i], &len);
                if (index == 0 && (unsigned int)len > longest_found)
                {
                    longest_found = len;
                    found = _token;
                }
            }
            else if (token.kind == MATCH_KIND_LITERAL)
            {
                if (token.text->size() <= longest_found)
                    continue;
                
                if (starts_with(&text[i], token.text->data()))
                {
                    longest_found = token.text->size();
                    found = _token;
                }
            }
            else
                assert(((void)"Broken internal grammar state!!!!!!", 0));
        }
        
        if (!found)
        {
            // append dummy token to token stream to signifify failure
            ret.push_back(MakeShared<Token>(Token{0, 0, i, line_index, row, column}));
            return ret;
        }
        
        auto mystr = MakeShared<String>(text.substr(i, longest_found));
        ret.push_back(MakeShared<Token>(Token{mystr, found->compiled_regex, i, line_index, row, column}));
        i += longest_found;
    }
    
    return ret;
}

struct ASTNode
{
    Vec<Shared<ASTNode>> children;
    size_t start_row = 1;
    size_t start_column = 1;
    size_t token_count = 0;
    Shared<String> text;
    bool is_token = false;
};

static inline void print_AST(Shared<ASTNode> node, size_t depth)
{
    auto indent = [&](){for (size_t i = 0; i < depth; i++) printf(" ");};
    
    indent();
    if (node->text)
    {
        if (node->children.size() > 0)
            printf("@%s\n", node->text->data());
        else
            printf("%s\n", node->text->data());
    }
    else
        printf("@anonymous\n");
    
    if (node->children.size() > 0)
    {
        indent();
        printf("@+\n");
        for (auto c : node->children)
        {
            print_AST(c, depth + 1);
        }
        indent();
        printf("@-\n");
    }
}
static inline void print_AST(Shared<ASTNode> node)
{
    print_AST(node, 0);
}

static Shared<ASTNode> ast_node_from_token(Shared<Token> token)
{
    return MakeShared<ASTNode>(ASTNode{{}, token->row, token->column, 1, token->text, true});
}

struct ParseRecord
{
    size_t token_index;
    Shared<String> node_name;
    bool operator==(const ParseRecord & other) const
    {
        return token_index == other.token_index && *node_name == *other.node_name;
    }
    bool operator<(const ParseRecord & other) const
    {
        if (*node_name < *other.node_name)
            return true;
        if (*other.node_name < *node_name)
            return false;
        return token_index < other.token_index;
    }
};

/*
template<>
struct std::hash<ParseRecord>
{
    std::size_t operator()(const ParseRecord & obj) const noexcept
    {
        size_t a = std::hash<size_t>{}(obj.token_index);
        size_t b = std::hash<String>{}(*obj.node_name);
        
        size_t x = a * 0x1061346952391 + b;
        return x;
    }
};
*/

static ListMap<ParseRecord, Shared<ASTNode>> parse_hits;
static ListSet<ParseRecord> parse_misses;
static size_t furthest = 0;
static ListSet<Shared<String>> furthest_maybes;

static void clear_parser_global_state()
{
    parse_hits.clear();
    parse_misses.clear();
}

static auto parse_with(const Vec<Shared<Token>> & tokens, size_t starting_token_index, Shared<GrammarPoint> node_type, size_t depth) -> Option<Shared<ASTNode>>
{
    const bool PARSER_DO_DEBUG_PRINT = false;
    
    auto indent = [&](){for (size_t i = 0; i < depth; i++) printf(" ");};
    
    if (node_type->name && PARSER_DO_DEBUG_PRINT)
    {
        indent();
        printf("+ checking for a... %s\n", node_type->name->data());
    }
    auto base_key = ParseRecord{starting_token_index, node_type->name};
    
    if (node_type->name && parse_hits.count(base_key) > 0)
        return parse_hits[base_key];
    if (node_type->name && parse_misses.count(base_key) > 0)
        return {};
    
    // try to find a form that matches
    for (auto & _form : node_type->forms)
    {
        //Vec<std::pair<size_t, Vec<ASTNode>>> backtrack_states;
        
        Vec<Vec<Shared<ASTNode>>> progress_scopes;
        Vec<Shared<ASTNode>> progress;
        
        size_t token_index = starting_token_index;
        
        auto form = &_form;
        // form matches if, after the below while loop, i == form->rules.size()
        // i advances whenever a subrule finishes matching
        size_t i = 0;
        size_t prev_i = i;
        
        size_t same_consec = 0;
        
        while (i < form->rules.size())
        {
            assert(token_index <= tokens.size());
            
            auto & rule = *form->rules[i];
            
            if (token_index > furthest)
            {
                furthest = token_index;
                furthest_maybes = {};
            }
            
            if (rule.kind == MATCH_KIND_LITERAL || rule.kind == MATCH_KIND_REGEX)
            {
                assert(rule.text);
                if (token_index == furthest)
                    furthest_maybes.insert(rule.text);
            }
            else if (token_index == furthest && rule.kind == MATCH_KIND_POINT)
                parse_with(tokens, token_index, rule.rule, depth + 1);
            
            if (token_index == tokens.size())
            {
                if (token_index == tokens.size() && (rule.qualifier == MATCH_QUAL_STAR || rule.qualifier == MATCH_QUAL_MAYBE))
                {
                    i += 1;
                    continue;
                }
                else
                {
                    i = form->rules.size();
                    break;
                }
            }
            
            assert(((void)"TODO", rule.lazy == false));
            assert(((void)"TODO", rule.greedy == false));
            
            size_t start_i = i;
            
            auto & token = tokens[token_index];
            assert(token);
            assert(token->text);
            
            if (//(rule.kind == MATCH_KIND_LITERAL || rule.kind == MATCH_KIND_REGEX) && 
                rule.text && PARSER_DO_DEBUG_PRINT)
            {
                indent();
                printf("comparing... `%s` vs `%s`\n", rule.text->data(), token->text->data());
            }
            if ((rule.kind == MATCH_KIND_LITERAL && *rule.text == *token->text) ||
                (rule.kind == MATCH_KIND_REGEX && rule.compiled_regex == token->from_regex))
            {
                if (PARSER_DO_DEBUG_PRINT)
                {
                    indent();
                    puts("match!");
                }
                token_index += 1;
                i += 1;
                progress.push_back(ast_node_from_token(token));
            }
            else if (rule.kind == MATCH_KIND_POINT)
            {
                assert(rule.rule);
                if (auto parse = parse_with(tokens, token_index, rule.rule, depth + 1))
                {
                    if (PARSER_DO_DEBUG_PRINT)
                    {
                        indent();
                        printf("found grammar point match! increasing token index by %zu...\n", (*parse)->token_count);
                    }
                    token_index += (*parse)->token_count;
                    i += 1;
                    progress.push_back(*parse);
                }
                else
                {
                    if (PARSER_DO_DEBUG_PRINT)
                    {
                        indent();
                        puts("failed point match");
                    }
                }
            }
            
            if (rule.qualifier == MATCH_QUAL_STAR)
            {
                if (start_i != i)
                    i = start_i;
                else
                    i = start_i + 1;
            }
            else if (rule.qualifier == MATCH_QUAL_PLUS)
            {
                if (start_i != i)
                    i = start_i;
                else if (same_consec == 0)
                    break;
                else
                    i = start_i + 1;
            }
            else if (rule.qualifier == MATCH_QUAL_MAYBE)
            {
                if (start_i == i)
                    i = start_i + 1;
            }
            else
            {
                if (start_i == i)
                    break;
            }
            
            if (prev_i == i)
                same_consec += 1;
            else
                same_consec = 0;
            prev_i = i;
        }
        
        if (i == form->rules.size() && starting_token_index < tokens.size())
        {
            ASTNode ret;
            ret.children = std::move(progress);
            ret.start_row = tokens[starting_token_index]->row;
            ret.start_column = tokens[starting_token_index]->column;
            ret.token_count = token_index - starting_token_index;
            ret.text = node_type->name;
            ret.is_token = false;
            if (PARSER_DO_DEBUG_PRINT)
            {
                indent();
                printf("- done checking! matched all %zu subrules of %s!!!\n", i, node_type->name->data());
            }
            if (node_type->no_tokens)
            {
                for (ptrdiff_t i = ret.children.size() - 1; i >= 0; i--)
                {
                    if (ret.children[i]->is_token)
                        ret.children.erase(ret.children.begin() + i);
                }
            }
            if (node_type->left_recursive && ret.children.size() > 1)
            {
                printf("\033[91mWARNING: HAVE NOT IMPLEMENTED LEFT-RECURSION ROTATION YET.\033[0m\n");
                //assert(((void)"TODO", 0));
            }
            
            auto ret_wrapped = MakeShared<ASTNode>(ret);
            
            if (node_type->name)
                parse_hits.insert(base_key, ret_wrapped);
            
            return {ret_wrapped};
        }
    }
    if (PARSER_DO_DEBUG_PRINT)
    {
        indent();
        puts("- done checking! not found...");
    }
    return {};
}

static auto parse_as(Grammar & grammar, const Vec<Shared<Token>> & tokens, const char * as_node_type) -> Option<Shared<ASTNode>>
{
    furthest = 0;
    
    assert(grammar.points.count(String(as_node_type)) > 0);
    auto point = grammar.points[String(as_node_type)];
    auto ret = parse_with(tokens, 0, point, 0);
    if (ret && (*ret)->token_count != tokens.size())
        ret = {};
    clear_parser_global_state();
    return ret;
}

static void print_tokenization_error(const Vec<Shared<Token>> & tokens, const String & text)
{
    printf("Tokenization failed. Parsing cannot continue.\n");
    
    auto token = tokens.back();
    printf("On line %zu at column %zu:\n", token->row, token->column);
    size_t i = token->line_index;
    while (i < text.size() && text[i] != '\n')
    {
        if (i == token->index)
            printf("\033[91m");
        else if (i >= text.size() || text[i] == ' ' || text[i] == '\t')
            printf("\033[0m");
        if (text[i] == '\t')
            printf("    ");
        else
            putc(text[i], stdout);
        i++;
    }
    printf("\033[0m");
    puts("");
    for (size_t n = 1; n < token->column; n++)
        putc(' ', stdout);
    printf("^---\n");
    puts("The grammar does not recognize the pointed-to text as valid, not even on a single-chunk level.");
}

static void print_parse_error(const Vec<Shared<Token>> & tokens, const String & text)
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
        size_t col = 0;
        while (i < text.size() && text[i] != '\n')
        {
            if (i == token->index)
                printf("\033[91m");
            else if (i == token->index + token->text->size())
                printf("\033[0m");
            if (text[i] == '\t')
            {
                printf(" ");
                col += 1;
                while (col % 4)
                {
                    printf(" ");
                    col += 1;
                }
            }
            else
            {
                putc(text[i], stdout);
                col += 1;
            }
            i++;
        }
        printf("\033[0m");
        puts("");
        for (size_t n = 1; n < token->column; n++)
            putc(' ', stdout);
        printf("^--- %zu\n", token->column);
    }
}

#endif // BBEL_GRAMMAR
