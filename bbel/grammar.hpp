#ifndef BBEL_GRAMMAR
#define BBEL_GRAMMAR

#include <cassert>
#include <cstring>
#include <cstdint>

#include <utility>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <optional>

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

static std::shared_ptr<Regex> regex_compile(const char* pattern)
{
    auto r = re_compile(pattern);
    auto ret = std::make_shared<Regex>(r);
    return ret;
}

static int regex_match(std::shared_ptr<Regex> pattern, const char * text, int * matchlength)
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
    // qualified matches are possessive-greedy by default
    MatchQualifier qualifier = MATCH_QUAL_DEFAULT;
    bool lazy = false;
    bool greedy = false;
};

struct GrammarForm {
    std::vector<std::shared_ptr<MatchingRule>> rules;
};

struct GrammarPoint {
    std::shared_ptr<std::string> name = 0;
    std::vector<GrammarForm> forms;
    GrammarPoint()
    {
        forms.push_back(GrammarForm{});
    }
};

static std::shared_ptr<MatchingRule> new_rule_regex(std::string regex)
{
    auto ret = MatchingRule { MATCH_KIND_REGEX, std::make_shared<std::string>(regex), 0, 0 };
    return std::make_shared<MatchingRule>(std::move(ret));
}
static std::shared_ptr<MatchingRule> new_rule_name(std::string name)
{
    auto ret = MatchingRule { MATCH_KIND_POINT, std::make_shared<std::string>(name), 0, 0 };
    return std::make_shared<MatchingRule>(std::move(ret));
}
static std::shared_ptr<MatchingRule> new_rule_point(std::shared_ptr<GrammarPoint> point)
{
    auto ret = MatchingRule { MATCH_KIND_POINT, 0, 0, point };
    return std::make_shared<MatchingRule>(std::move(ret));
}
static std::shared_ptr<MatchingRule> new_rule_text(std::string text)
{
    auto ret = MatchingRule { MATCH_KIND_LITERAL, std::make_shared<std::string>(text), 0, 0 };
    return std::make_shared<MatchingRule>(std::move(ret));
}

struct Grammar
{
    std::unordered_map<std::string, std::shared_ptr<GrammarPoint>> points;
    std::vector<std::shared_ptr<MatchingRule>> tokens;
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
    
    std::unordered_map<std::string, std::shared_ptr<GrammarPoint>> ret;
    std::unordered_set<std::shared_ptr<GrammarPoint>> all_points;
    std::vector<std::shared_ptr<MatchingRule>> tokens;
    
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
    
    auto current_point = std::make_shared<GrammarPoint>();
    
    while (i < len)
    {
        while (line_is_empty() && text[i] != 0)
        {
            if (current_point->name)
            {
                ret.insert({*current_point->name, current_point});
                all_points.insert(current_point);
                current_point = std::make_shared<GrammarPoint>();
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
            current_point->name = std::make_shared<std::string>(name);
            mode = MODE_FORMS;
            
            go_to_next_line();
        }
        else if (mode == MODE_FORMS)
        {
            //printf("form in... %s\n", current_point->name->data());
            std::vector<std::shared_ptr<GrammarPoint>> point_stack;
            
            while (!line_is_empty())
            {
                auto & form = current_point->forms.back();
                
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
                    current_point = std::make_shared<GrammarPoint>();
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
        ret.insert({*current_point->name, current_point});
        all_points.insert(current_point);
    }
    
    std::sort(tokens.begin(), tokens.end(), [](std::shared_ptr<MatchingRule> a, std::shared_ptr<MatchingRule> b)
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
                    auto s = std::string("^") + *rule->text;
                    rule->compiled_regex = regex_compile(s.data());
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
    
    return {ret, tokens};
}

struct Token {
    std::shared_ptr<std::string> text = 0;
    std::shared_ptr<Regex> from_regex = 0;
    size_t index = 0;
    size_t line_index = 0;
    size_t row = 0;
    size_t column = 0;
};

static std::vector<std::shared_ptr<Token>> tokenize(const Grammar & grammar, const char * _text)
{
    const std::vector<std::shared_ptr<MatchingRule>> & tokens = grammar.tokens;
    std::vector<std::shared_ptr<Token>> ret;
    
    std::string text = _text;
    
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
        
        bool found = false;
        for (auto _token : tokens)
        {
            auto & token = *_token;
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
                    auto mystr = std::make_shared<std::string>(text.substr(i, len));
                    ret.push_back(std::make_shared<Token>(Token{mystr, token.compiled_regex, i, line_index, row, column}));
                    
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
                    auto mystr = std::make_shared<std::string>(text.substr(i, token.text->size()));
                    ret.push_back(std::make_shared<Token>(Token{mystr, 0, i, line_index, row, column}));
                    
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

struct ASTNode
{
    std::vector<std::shared_ptr<ASTNode>> children;
    size_t start_row = 1;
    size_t start_column = 1;
    size_t token_count = 0;
    std::shared_ptr<std::string> text;
};

void print_AST(std::shared_ptr<ASTNode> node, size_t depth)
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
void print_AST(std::shared_ptr<ASTNode> node)
{
    print_AST(node, 0);
}

std::shared_ptr<ASTNode> ast_node_from_token(std::shared_ptr<Token> token)
{
    return std::make_shared<ASTNode>(ASTNode{{}, token->row, token->column, 1, token->text});
}

struct ParseRecord
{
    size_t token_index;
    std::shared_ptr<std::string> node_name;
};

template<>
struct std::hash<ParseRecord>
{
    std::size_t operator()(const ParseRecord & obj) const noexcept
    {
        size_t a = std::hash<size_t>{}(obj.token_index);
        size_t b = std::hash<std::string_view>{}(*obj.node_name);
        
        size_t x = a * 0x1061346952391 + b;
        return x;
    }
};

static std::unordered_map<ParseRecord, std::shared_ptr<ASTNode>> parse_hits;
static std::unordered_set<ParseRecord> parse_misses;
static size_t furthest = 0;
static std::unordered_set<std::shared_ptr<std::string>> furthest_maybes;

static void clear_parser_global_state()
{
    parse_hits.clear();
    parse_misses.clear();
}

static auto parse_with(const std::vector<std::shared_ptr<Token>> & tokens, size_t starting_token_index, std::shared_ptr<GrammarPoint> node_type, size_t depth) -> std::optional<std::shared_ptr<ASTNode>>
{
    const bool PARSER_DO_DEBUG_PRINT = false;
    
    auto indent = [&](){for (size_t i = 0; i < depth; i++) printf(" ");};
    
    if (node_type->name && PARSER_DO_DEBUG_PRINT)
    {
        indent();
        printf("+ checking for a... %s\n", node_type->name->data());
    }
    auto base_key = ParseRecord{starting_token_index, node_type->name};
    // try to find a form that matches
    for (auto & _form : node_type->forms)
    {
        //std::vector<std::pair<size_t, std::vector<ASTNode>>> backtrack_states;
        
        std::vector<std::vector<std::shared_ptr<ASTNode>>> progress_scopes;
        std::vector<std::shared_ptr<ASTNode>> progress;
        
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
            if (PARSER_DO_DEBUG_PRINT)
            {
                indent();
                printf("- done checking! matched all %zu subrules of %s!!!\n", i, node_type->name->data());
            }
            return {std::make_shared<ASTNode>(ret)};
        }
    }
    if (PARSER_DO_DEBUG_PRINT)
    {
        indent();
        puts("- done checking! not found...");
    }
    return {};
}

static auto parse_as(Grammar & grammar, const std::vector<std::shared_ptr<Token>> & tokens, const char * as_node_type) -> std::optional<std::shared_ptr<ASTNode>>
{
    furthest = 0;
    
    assert(grammar.points.count(std::string(as_node_type)) > 0);
    auto point = grammar.points[std::string(as_node_type)];
    auto ret = parse_with(tokens, 0, point, 0);
    if (ret && (*ret)->token_count != tokens.size())
        ret = {};
    clear_parser_global_state();
    return ret;
}

#endif // BBEL_GRAMMAR
