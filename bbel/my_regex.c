#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

const static int RXTOK_FLAG_DOT_NEWLINES = 1;

const static uint8_t RXTOK_KIND_NORMAL      = 0;
const static uint8_t RXTOK_KIND_OPEN        = 1;
const static uint8_t RXTOK_KIND_NCOPEN      = 2;
const static uint8_t RXTOK_KIND_CLOSE       = 3;
const static uint8_t RXTOK_KIND_OR          = 4;
const static uint8_t RXTOK_KIND_CARET       = 5;
const static uint8_t RXTOK_KIND_DOLLAR      = 6;
const static uint8_t RXTOK_KIND_BOUND       = 7;
const static uint8_t RXTOK_KIND_NBOUND      = 8;
const static uint8_t RXTOK_KIND_END         = 9;

const static uint8_t RXTOK_MODE_POSSESSIVE  = 1;
const static uint8_t RXTOK_MODE_LAZY        = 2;
const static uint8_t RXTOK_MODE_INVERTED    = 128; // temporary; gets cleared later

typedef struct _RegexToken {
    uint8_t kind;
    uint8_t mode;
    uint16_t count_lo;
    uint16_t count_hi; // 0 means no limit
    uint16_t mask[16]; // for groups: mask 0 stores group-with-quantifier number (quantifiers are +, *, ?, {n}, {n,}, or {n,m})
    int16_t pair_offset; // from ( or ), offset in token list to matching paren. TODO: move into mask maybe
} RegexToken;

/// Returns a negative number on failure:
/// -1: Regex string is invalid or using unsupported features or too long.
/// -2: Provided buffer not long enough. Give up, or reallocate with more length and retry.
/// Returns 0 on success.
/// On call, token_count pointer must point to the number of tokens that can be written to the tokens buffer.
/// On successful return, the number of actually used tokens is written to token_count.
/// Sets token_count to zero if a regex is not created but no error happened (e.g. empty pattern).
/// Flags: Not yet used.
/// SAFETY: Pattern must be null-terminated.
/// SAFETY: tokens buffer must have at least the input token_count number of RegexToken objects. They are allowed to be uninitialized.
static int regex_parse(const char * pattern, RegexToken * tokens, int16_t * token_count, int32_t flags)
{
    int64_t tokens_len = *token_count;
    size_t pattern_len = strlen(pattern);
    if (pattern_len == 0)
    {
        *token_count = 0;
        return 0;
    }
    
    if (token_count == 0)
        return -2;
    
    // 0: normal
    // 1: just saw a backslash
    int esc_state = 0;
    
    // 0: init
    // 1: normal
    // 2: in char class, initial state
    // 3: in char class, but possibly looking for a range marker
    // 4: in char class, but just saw a range marker
    // 5: immediately after quantifiable token
    // 6: immediately after quantifier
    
    const int STATE_NORMAL    = 1;
    const int STATE_QUANT     = 2;
    const int STATE_MODE      = 3;
    const int STATE_CC_INIT   = 4;
    const int STATE_CC_NORMAL = 5;
    const int STATE_CC_RANGE  = 6;
    int state = STATE_NORMAL;
    
    int char_class_mem = -1;
    
    RegexToken token;
    
    #define _REGEX_CLEAR_TOKEN(TOKEN) { memset(&(TOKEN), 0, sizeof(RegexToken)); token.count_lo = 1; token.count_hi = 1; }
    
    _REGEX_CLEAR_TOKEN(token);
    
    #define _REGEX_DO_INVERT() { \
        for (int n = 0; n < 16; n++) \
            token.mask[n] = ~token.mask[n]; \
        token.mode &= ~RXTOK_MODE_INVERTED; \
    }
    
    int16_t k = 0;
    
    #define _REGEX_PUSH_TOKEN() { \
        if (k == 0 || tokens[k-1].kind != token.kind || (token.kind != RXTOK_KIND_BOUND && token.kind != RXTOK_KIND_NBOUND)) \
        { \
            if (token.mode & RXTOK_MODE_INVERTED) _REGEX_DO_INVERT() \
            if (k >= tokens_len) \
            { \
                puts("buffer overflow"); \
                return -2; \
            } \
            tokens[k++] = token; \
            _REGEX_CLEAR_TOKEN(token); \
        } \
    }
    
    #define _REGEX_SET_MASK(byte) { token.mask[((uint8_t)byte)>>4] |= 1 << ((uint8_t)byte & 0xF); }
    #define _REGEX_SET_MASK_ALL() { \
        for (int n = 0; n < 16; n++) \
            token.mask[n] = 0xFFFF; \
    }
    
    // start with an invisible non-capturing group specifier
    // (this allows the matcher to not need to have a special root-level alternation operator case)
    token.kind = RXTOK_KIND_NCOPEN;
    token.count_lo = 0;
    token.count_hi = 0;

    int paren_count = 0;
    
    for (size_t i = 0; i < pattern_len; i++)
    {
        char c = pattern[i];
        if (state == STATE_QUANT)
        {
            state = STATE_MODE;
            if (c == '?')
            {
                token.count_lo = 0;
                token.count_hi = 1;
                continue;
            }
            else if (c == '+')
            {
                token.count_lo = 1;
                token.count_hi = 0; // unlimited
                continue;
            }
            else if (c == '*')
            {
                token.count_lo = 0;
                token.count_hi = 0; // unlimited
                continue;
            }
            else if (c == '{')
            {
                if (pattern[i+1] == 0 || pattern[i+1] < '0' || pattern[i+1] > '9')
                    state = STATE_NORMAL;
                else
                {
                    i += 1;
                    uint32_t val = 0;
                    while (pattern[i] >= '0' && pattern[i] <= '9')
                    {
                        val *= 10;
                        val += (uint32_t)(pattern[i] - '0');
                        if (val > 0xFFFF)
                        {
                            puts("quantifier range too long");
                            return -1; // unsupported length
                        }
                        i += 1;
                    }
                    token.count_lo = val;
                    token.count_hi = val;
                    if (pattern[i] == ',')
                    {
                        token.count_hi = 0; // unlimited
                        i += 1;
                        
                        if (pattern[i] >= '0' && pattern[i] <= '9')
                        {
                            uint32_t val2 = 0;
                            while (pattern[i] >= '0' && pattern[i] <= '9')
                            {
                                val2 *= 10;
                                val2 += (uint32_t)(pattern[i] - '0');
                                if (val2 > 0xFFFF)
                                {
                                    puts("quantifier range too long");
                                    return -1; // unsupported length
                                }
                                i += 1;
                            }
                            if (val2 < val)
                            {
                                puts("quantifier range is backwards");
                                return -1; // unsupported length
                            }
                            token.count_hi = val2;
                            if (val2 == 0) // unmatchable quantifier. deliberately create unmatchable token
                            {
                                token.count_lo = 2;
                                token.count_hi = 1;
                            }
                        }
                    }
                    else if (val == 0) // unmatchable quantifier. deliberately create unmatchable token
                    {
                        token.count_lo = 2;
                        token.count_hi = 1;
                    }
                    
                    if (pattern[i] == '}')
                    {
                        // quantifier range parsed successfully
                        continue;
                    }
                    else
                    {
                        puts("quantifier range syntax broken (no terminator)");
                        return -1;
                    }
                }
            }
        }
        
        if (state == STATE_MODE)
        {
            state = STATE_NORMAL;
            if (c == '?')
            {
                token.mode |= RXTOK_MODE_LAZY;
                continue;
            }
            else if (c == '+')
            {
                token.mode |= RXTOK_MODE_POSSESSIVE;
                continue;
            }
        }
        
        if (state == STATE_NORMAL)
        {
            if (esc_state == 1)
            {
                esc_state = 0;
                if (c == 'n')
                    _REGEX_SET_MASK('\n')
                else if (c == 'r')
                    _REGEX_SET_MASK('\r')
                else if (c == 't')
                    _REGEX_SET_MASK('\t')
                else if (c == 'v')
                    _REGEX_SET_MASK('\v')
                else if (c == 'f')
                    _REGEX_SET_MASK('\f')
                else if (c == 'd' || c == 's' || c == 'w' ||
                         c == 'D' || c == 'S' || c == 'W')
                {
                    uint8_t is_upper = c <= 'Z';
                    assert(((void)"FIXME need to rewrite these", 0));
                    if (is_upper)
                        c += 0x20;
                    if (c == 'd' || c == 'w')
                    {
                        token.mask[6] |= is_upper ? ~0xFF : 0xFF; // 0~7
                        token.mask[7] |= is_upper ? ~3 : 3; // 8-9
                    }
                    if (c == 's')
                    {
                        token.mask[1] |= is_upper ? ~0x3E : 0x3E; // \t-\r (includes \n, \v, and \f in the middle. 5 enabled bits.)
                        token.mask[4] |= is_upper ? ~1 : 1; // ' '
                    }
                    if (c == 'w')
                    {
                        token.mask[8] |= is_upper ? ~0xFE : 0xFE; // A-G
                        token.mask[9] |= is_upper ? ~0xFF : 0xFF; // H-O
                        token.mask[10] |= is_upper ? ~0xFF : 0xFF; // P-W
                        token.mask[11] |= is_upper ? ~7 : 7; // X-Z
                    }
                }
                else if (c == '{' || c == '}' ||
                         c == '[' || c == ']' || c == '-' ||
                         c == '(' || c == ')' ||
                         c == '|' || c == '^' || c == '$' ||
                         c == '*' || c == '+' || c == '?' || c == ':' ||
                         c == '.' || c == '/' || c == '\\')
                {
                    _REGEX_SET_MASK(c)
                    state = STATE_QUANT;
                }
                else if (c == 'b')
                {
                    token.kind = RXTOK_KIND_BOUND;
                    state = STATE_NORMAL;
                }
                else if (c == 'B')
                {
                    token.kind = RXTOK_KIND_NBOUND;
                    state = STATE_NORMAL;
                }
                else
                {
                    puts("unsupported escape sequence");
                    return -1; // unknown/unsupported escape sequence
                }
            }
            else
            {
                _REGEX_PUSH_TOKEN()
                if (c == '\\')
                {
                    esc_state = 1;
                }
                else if (c == '[')
                {
                    state = STATE_CC_INIT;
                    char_class_mem = -1;
                    token.kind = RXTOK_KIND_NORMAL;
                    if (pattern[i + 1] == '^')
                    {
                        token.mode |= RXTOK_MODE_INVERTED;
                        i += 1;
                    }
                }
                else if (c == '(')
                {
                    paren_count += 1;
                    state = STATE_NORMAL;
                    token.kind = RXTOK_KIND_OPEN;
                    token.count_lo = 0;
                    token.count_hi = 0;
                    if (pattern[i + 1] == '?' && pattern[i + 2] == ':')
                    {
                        token.kind = RXTOK_KIND_NCOPEN;
                        i += 2;
                    }
                }
                else if (c == ')')
                {
                    paren_count -= 1;
                    if (paren_count < 0 || k == 0)
                        return -1; // unbalanced parens
                    token.kind = RXTOK_KIND_CLOSE;
                    state = STATE_QUANT;
                    
                    int balance = 0;
                    ptrdiff_t found = -1;
                    for (ptrdiff_t l = k - 1; l >= 0; l--)
                    {
                        if (tokens[l].kind == RXTOK_KIND_NCOPEN || tokens[l].kind == RXTOK_KIND_OPEN)
                        {
                            assert(balance >= 0);
                            if (balance == 0)
                            {
                                found = l;
                                break;
                            }
                            else
                                balance -= 1;
                        }
                        else if (tokens[l].kind == RXTOK_KIND_CLOSE)
                            balance += 1;
                    }
                    if (found == -1)
                        return -1; // unbalanced parens
                    ptrdiff_t diff = k - found;
                    if (diff > 32767)
                        return -1; // too long
                    token.pair_offset = -diff;
                    tokens[found].pair_offset = diff;
                }
                else if (c == '?' || c == '+' || c == '*' || c == '{')
                {
                    puts("quantifier in non-quantifier context");
                    return -1; // quantifier in non-quantifier context
                }
                else if (c == '.')
                {
                    //puts("setting ALL of mask...");
                    _REGEX_SET_MASK_ALL();
                    if (!(flags & RXTOK_FLAG_DOT_NEWLINES))
                    {
                        token.mask[1] ^= 0x04; // \n
                        token.mask[1] ^= 0x20; // \r
                    }
                    state = STATE_QUANT;
                }
                else if (c == '^')
                {
                    token.kind = RXTOK_KIND_CARET;
                    state = STATE_NORMAL;
                }
                else if (c == '$')
                {
                    token.kind = RXTOK_KIND_DOLLAR;
                    state = STATE_NORMAL;
                }
                else if (c == '|')
                {
                    token.kind = RXTOK_KIND_OR;
                    state = STATE_NORMAL;
                }
                else
                {
                    _REGEX_SET_MASK(c);
                    state = STATE_QUANT;
                }
            }
        }
        else if (state == STATE_CC_INIT || state == STATE_CC_NORMAL || state == STATE_CC_RANGE)
        {
            if (c == '\\' && esc_state == 0)
            {
                esc_state = 1;
                continue;
            }
            uint8_t esc_c = 0;
            if (esc_state == 1)
            {
                esc_state = 0;
                if (c == 'n')
                    esc_c = '\n';
                else if (c == 'r')
                    esc_c = '\r';
                else if (c == 't')
                    esc_c = '\t';
                else if (c == 'v')
                    esc_c = '\v';
                else if (c == 'f')
                    esc_c = '\f';
                else if (c == '{' || c == '}' ||
                         c == '[' || c == ']' || c == '-' ||
                         c == '(' || c == ')' ||
                         c == '|' || c == '^' || c == '$' ||
                         c == '*' || c == '+' || c == '?' || c == ':' ||
                         c == '.' || c == '/' || c == '\\')
                {
                    esc_c = c;
                }
                else if (c == 'd' || c == 's' || c == 'w' ||
                         c == 'D' || c == 'S' || c == 'W')
                {
                    if (state == STATE_CC_RANGE)
                    {
                        puts("tried to use a shorthand as part of a range");
                        return -1; // range shorthands can't be part of a range
                    }
                    uint8_t is_upper = c <= 'Z';
                    if (is_upper)
                        c += 0x20;
                    if (c == 'd' || c == 'w')
                    {
                        token.mask[6] |= is_upper ? ~0xFF : 0xFF; // 0~7
                        token.mask[7] |= is_upper ? ~3 : 3; // 8-9
                    }
                    if (c == 's')
                    {
                        token.mask[1] |= is_upper ? ~0x3E : 0x3E; // \t-\r (includes \n, \v, and \f in the middle. 5 enabled bits.)
                        token.mask[4] |= is_upper ? ~1 : 1; // ' '
                    }
                    if (c == 'w')
                    {
                        token.mask[8] |= is_upper ? ~0xFE : 0xFE; // A-G
                        token.mask[9] |= is_upper ? ~0xFF : 0xFF; // H-O
                        token.mask[10] |= is_upper ? ~0xFF : 0xFF; // P-W
                        token.mask[11] |= is_upper ? ~7 : 7; // X-Z
                    }
                    
                    char_class_mem = -1; // range shorthands can't be part of a range
                    continue;
                }
                else
                {
                    printf("unknown/unsupported escape sequence in character class (\\%c)\n", c);
                    return -1; // unknown/unsupported escape sequence
                }
            }
            if (state == STATE_CC_INIT)
            {
                char_class_mem = c;
                _REGEX_SET_MASK(c);
                state = STATE_CC_NORMAL;
            }
            else if (state == STATE_CC_NORMAL)
            {
                if (c == ']' && esc_c == 0)
                {
                    char_class_mem = -1;
                    state = STATE_QUANT;
                    continue;
                }
                else if (c == '-' && esc_c == 0 && char_class_mem >= 0)
                {
                    state = STATE_CC_RANGE;
                    continue;
                }
                else
                {
                    char_class_mem = c;
                    _REGEX_SET_MASK(c);
                    state = STATE_CC_NORMAL;
                }
            }
            else if (state == STATE_CC_RANGE)
            {
                if (c == ']' && esc_c == 0)
                {
                    char_class_mem = -1;
                    _REGEX_SET_MASK('-');
                    state = STATE_QUANT;
                    continue;
                }
                else
                {
                    if (char_class_mem == -1)
                    {
                        puts("character class range is broken");
                        return -1; // probably tried to use a character class shorthand as part of a range
                    }
                    if ((uint8_t)c < char_class_mem)
                    {
                        puts("character class range is misordered");
                        return -1; // range is in wrong order
                    }
                    //printf("enabling char class from %d to %d...\n", char_class_mem, c);
                    for (uint8_t i = c; i > char_class_mem; i--)
                        _REGEX_SET_MASK(i);
                    state = STATE_CC_NORMAL;
                    char_class_mem = -1;
                }
            }
        }
        else
            assert(0);
    }
    if (paren_count > 0)
    {
        puts("(paren_count > 0)");
        return -1; // unbalanced parens
    }
    if (esc_state != 0)
    {
        puts("(esc_state != 0)");
        return -1; // open escape sequence
    }
    if (state >= STATE_CC_INIT)
    {
        puts("(state >= STATE_CC_INIT)");
        return -1; // open character class
    }
    
    _REGEX_PUSH_TOKEN()
    
    // add invisible non-capturing group specifier
    token.kind = RXTOK_KIND_CLOSE;
    token.count_lo = 1;
    token.count_hi = 1;
    _REGEX_PUSH_TOKEN();
    
    // add end token (tells matcher that it's done)
    token.kind = RXTOK_KIND_END;
    _REGEX_PUSH_TOKEN();
    
    tokens[0].pair_offset = k - 2;
    tokens[k-2].pair_offset = -(k - 2);
    
    *token_count = k;
    
    // copy quantifiers from )s to (s (so (s know whether they're optional)
    // also take the opportunity to smuggle "quantified group index" into the mask field for the )
    size_t n = 0;
    for (int16_t k2 = 0; k2 < k; k2++)
    {
        if (tokens[k2].kind == RXTOK_KIND_CLOSE)
        {
            tokens[k2].mask[0] = n++;
            //if (n > 65535)
            if (n > 1024)
                return -1; // too many quantified groups
            
            int16_t k3 = k2 + tokens[k2].pair_offset;
            tokens[k3].count_lo = tokens[k2].count_lo;
            tokens[k3].count_hi = tokens[k2].count_hi;
            tokens[k3].mask[0] = tokens[k2].mask[0];
            tokens[k3].mode = tokens[k2].mode;
        }
    }
    
    #undef _REGEX_PUSH_TOKEN
    #undef _REGEX_SET_MASK
    #undef _REGEX_CLEAR_TOKEN
    
    return 0;
}

typedef struct _RegexMatcherState {
    unsigned int k;
    uint16_t group_state; // quantified group temp state
    size_t i;
    uint64_t range_min;
    uint64_t range_max;
} RegexMatcherState;

typedef struct RegexGreedState {
    size_t i;
    size_t k;
} RegexGreedState;

// NOTE: undef'd later
#define _REGEX_CHECK_MASK(K, byte) (!!(tokens[K].mask[((uint8_t)byte)>>4] & (1 << ((uint8_t)byte & 0xF))))

// Returns match length if text starts with a regex match.
// Returns -1 if the text doesn't start with a regex match.
// Returns -2 if the matcher ran out of memory.
// Returns -3 if the regex is somehow invalid.
// SAFETY: The text variable must be null-terminated.
// SAFETY: Tokens array must be terminated by a RXTOK_KIND_END token (done by default by regex_parse).
static int64_t regex_match(const RegexToken * tokens, const char * text)
{
    (void)text;
    
    size_t tokens_len = 0;
    unsigned int k = 0;
    
    // quantified group state
    int32_t q_group_state[1024];
    while (tokens[k].kind != RXTOK_KIND_END)
    {
        k += 1;
        if (tokens[k].kind == RXTOK_KIND_CLOSE)
        {
            if (tokens[k].mask[0] >= 1024)
                return -2; // OOM: too many quantified groups
            
            q_group_state[tokens[k].mask[0]] = 0;
        }
    }
    
    tokens_len = k;
    
    const uint16_t stack_size_max = 1024;
    
    RegexGreedState greed_stack[stack_size_max];
    RegexMatcherState rewind_stack[stack_size_max];
    uint16_t greed_stack_n = 0;
    uint16_t stack_n = 0;
    
    size_t i = 0;
    
    uint64_t range_min = 0;
    uint64_t range_max = 0;
    uint8_t just_rewinded = 0;
    
    #define _P_TEXT_HIGHLIGHTED() { \
        printf("\033[91m"); \
        for (size_t q = 0; q < i; q++) printf("%c", text[q]); \
        printf("\033[0m"); \
        for (size_t q = i; text[q] != 0; q++) printf("%c", text[q]); \
        printf("\n"); \
    }
    
    #define _GREED_PUSH(I, K) { \
        if (greed_stack_n >= stack_size_max) \
            return -2; \
        greed_stack[greed_stack_n].i = (I); \
        greed_stack[greed_stack_n].k = (K); \
        greed_stack_n += 1; \
    }
    
    #define _GREED_POP() { \
        if (greed_stack_n == 0) \
            return -1; \
        if (k == greed_stack[greed_stack_n - 1].k) \
            i = greed_stack[--greed_stack_n].i; \
    }
    
    #define _REWIND_DO_SAVE(K) { \
        if (stack_n >= stack_size_max) \
            return -2; \
        RegexMatcherState s; \
        s.i = i; \
        s.k = (K); \
        s.range_min = range_min; \
        s.range_max = range_max; \
        if (tokens[s.k].kind == RXTOK_KIND_CLOSE) \
            s.group_state = q_group_state[tokens[s.k].mask[0]]; \
        rewind_stack[stack_n++] = s; \
        printf("-- saving rewind state %u %zd %zu %zu\n", s.k, i, range_min, range_max); \
        _P_TEXT_HIGHLIGHTED() \
    }
    
    #define _REWIND_OR_ABORT() { \
        if (stack_n == 0) \
            return -1; \
        stack_n -= 1; \
        just_rewinded = 1; \
        range_min = rewind_stack[stack_n].range_min; \
        range_max = rewind_stack[stack_n].range_max; \
        i = rewind_stack[stack_n].i; \
        k = rewind_stack[stack_n].k; \
        if (tokens[k].kind == RXTOK_KIND_CLOSE) \
            q_group_state[tokens[k].mask[0]] = rewind_stack[stack_n].group_state; \
        printf("-- rewound to k %u i %zd rl %zu rh %zu (kind %d)\n", k, i, range_min, range_max, tokens[k].kind); \
        _P_TEXT_HIGHLIGHTED() \
        k -= 1; \
    }
    // the -= 1 is because of the k++ in the for loop
    
    for (k = 0; k < tokens_len; k++)
    {
        //printf("k: %u\ti: %zu\n", k, i);
        if (tokens[k].kind == RXTOK_KIND_CARET)
        {
            if (i != 0)
                _REWIND_OR_ABORT()
            continue;
        }
        else if (tokens[k].kind == RXTOK_KIND_DOLLAR)
        {
            if (i == 0 || text[i-1] != 0)
                _REWIND_OR_ABORT()
            continue;
        }
        else if (tokens[k].kind == RXTOK_KIND_BOUND || tokens[k].kind == RXTOK_KIND_NBOUND)
        {
            assert(((void)"TODO word boundary token", 0));
        }
        else
        {
            // deliberately unmatchable token (e.g. a{0}, a{0,0})
            if (tokens[k].count_lo > tokens[k].count_hi && tokens[k].count_hi > 0)
            {
                if (tokens[k].kind == RXTOK_KIND_OPEN || tokens[k].kind == RXTOK_KIND_NCOPEN)
                    k += tokens[k].pair_offset;
                else
                    k += 1;
                continue;
            }
            
            if (tokens[k].kind == RXTOK_KIND_OPEN || tokens[k].kind == RXTOK_KIND_NCOPEN)
            {
                //puts("hit OPEN");
                if (!just_rewinded)
                {
                    // if we're lazy and the min length is 0, we need to try the non-group case first
                    if ((tokens[k].mode & RXTOK_MODE_LAZY) && tokens[k].count_lo == 0)
                    {
                        range_min = 0;
                        range_max = 0;
                        puts("(saving in paren 2)");
                        _REWIND_DO_SAVE(k)
                        k += tokens[k].pair_offset; // automatic += 1 will put us past the matching )
                        continue;
                    }
                    else
                    {
                        range_min = 1;
                        range_max = 0;
                        puts("(saving in paren)");
                        _REWIND_DO_SAVE(k)
                        continue;
                    }
                }
                else
                {
                    just_rewinded = 0;
                    
                    size_t orig_k = k;
                    
                    if (range_min != 0)
                    {
                        k += range_min;
                        while (tokens[k].kind != RXTOK_KIND_END && tokens[k].kind != RXTOK_KIND_OR && tokens[k].kind != RXTOK_KIND_CLOSE)
                            k += 1;
                        
                        if (tokens[k].kind == RXTOK_KIND_END) // unbalanced parens
                            return -4;
                        
                        if (tokens[k].kind == RXTOK_KIND_CLOSE)
                        {
                            puts("!!~!~!~~~~!!~~!~   hit CLOSE. rewinding");
                            _REWIND_OR_ABORT() // to last point before group
                            continue;
                        }
                        
                        assert(tokens[k].kind == RXTOK_KIND_OR);
                    }
                    
                    ptrdiff_t k_diff = k - orig_k;
                    range_min = k_diff + 1;
                    
                    puts("(saving in paren after rewinding)");
                    _REWIND_DO_SAVE(k - k_diff)
                    
                    puts("!!~!~!~~~~!!~~!~   hit OR. continuing");
                    continue;
                }
            }
            else if (tokens[k].kind == RXTOK_KIND_CLOSE)
            {
                if (tokens[k].count_lo != 1 || tokens[k].count_hi != 1)
                {
                    if (!just_rewinded)
                    {
                        if (tokens[k].mode & RXTOK_MODE_LAZY)
                        {
                            printf("?!?!?!?! %d\n", q_group_state[tokens[k].mask[0]]);
                            // can only match at most once; do nothing, advance to next
                            if (tokens[k].count_hi == 1)
                                q_group_state[tokens[k].mask[0]] = 0;
                            // haven't hit limit, retry group
                            else if (tokens[k].count_hi == 0 || q_group_state[tokens[k].mask[0]] < tokens[k].count_hi)
                            {
                                q_group_state[tokens[k].mask[0]] += 1;
                                
                                range_min = tokens[k].count_lo;
                                range_max = tokens[k].count_hi;
                                
                                _REWIND_DO_SAVE(k)
                                
                                k += tokens[k].pair_offset; // back to start of group
                                k -= 1; // ensure we actually hit the group node next and not the node after it
                            }
                            // lot allowed to continue matching, go back
                            else
                                _REWIND_OR_ABORT()
                        }
                        else // greedy
                        {
                            // all the interesting logic for greed is handled in the rewind case
                            q_group_state[tokens[k].mask[0]] += 1;
                            _GREED_PUSH(i, k)
                            _REWIND_DO_SAVE(k)
                            k += tokens[k].pair_offset; // back to start of group
                            k -= 1; // ensure we actually hit the group node next and not the node after it
                        }
                    }
                    else
                    {
                        just_rewinded = 0;
                        
                        if (tokens[k].mode & RXTOK_MODE_LAZY)
                        {
                            printf("???? %d\n", q_group_state[tokens[k].mask[0]]);
                            if (q_group_state[tokens[k].mask[0]] >= tokens[k].count_lo)
                            {
                                // advance forward from group
                                puts("------ 05195329325932");
                                q_group_state[tokens[k].mask[0]] = 0;
                            }
                            else
                            {
                                puts("----- xcbkjhwri...");
                                _REWIND_OR_ABORT()
                            }
                        }
                        else
                        {
                            unsigned int old_k = k;
                            size_t old_i = i;
                            _GREED_POP()
                            // first time backtracking from this greedy bit.
                            // this means we just tried matching this group and it failed.
                            // let's try to match the rest of the regex past this group.
                            // we don't need to set any more state to do this; greed_pop sets it up properly.
                            if (i == old_i)
                            {
                                // if we're outside the acceptable range, rewind
                                if (q_group_state[tokens[k].mask[0]] < tokens[k].count_lo ||
                                    (q_group_state[tokens[k].mask[0]] > tokens[k].count_hi && tokens[k].count_hi != 0))
                                {
                                    _REWIND_OR_ABORT()
                                    continue;
                                }
                                // otherwise keep going
                                q_group_state[tokens[k].mask[0]] = 0;
                            }
                            // second time. need to start actually backtracking
                            else if (k == old_k)
                            {
                                _REWIND_OR_ABORT()
                                while (k + 1 != old_k)
                                    _REWIND_OR_ABORT()
                                k += 1; // _REWIND_OR_ABORT() subtracts 1 from k; we don't want that here
                                
                                assert(q_group_state[tokens[k].mask[0]] > 0);
                                q_group_state[tokens[k].mask[0]] -= 1;
                                _REWIND_DO_SAVE(k)
                            }
                            else
                                _REWIND_OR_ABORT()
                        }
                    }
                }
            }
            else if (tokens[k].kind == RXTOK_KIND_OR)
            {
                assert(((void)"FIXME", !just_rewinded));
                puts("successfully hit OR. skipping until end of group");
                while (tokens[k].kind != RXTOK_KIND_END && tokens[k].kind != RXTOK_KIND_CLOSE)
                    k += 1;
                k -= 1;
            }
            else if (tokens[k].kind == RXTOK_KIND_NORMAL)
            {
                //puts("a");
                if (!just_rewinded)
                {
                    size_t n = 0;
                    // do whatever the obligatory minimum amount of matching is
                    while (n < tokens[k].count_lo && _REGEX_CHECK_MASK(k, text[i]))
                    {
                        printf("match?? (%c)\n", text[i]);
                        i += 1;
                        n += 1;
                    }
                    if (n < tokens[k].count_lo)
                    {
                        puts("non-match A. rewinding");
                        _REWIND_OR_ABORT()
                        continue;
                    }
                    
                    if (tokens[k].mode & RXTOK_MODE_LAZY)
                    {
                        range_min = n;
                        range_max = tokens[k].count_hi;
                        _REWIND_DO_SAVE(k)
                    }
                    else
                    {
                        size_t limit = tokens[k].count_hi;
                        if (limit == 0)
                            limit = ~limit;
                        range_min = n;
                        while (_REGEX_CHECK_MASK(k, text[i]) && text[i] != 0 && n < limit)
                        {
                            printf("match!! (%c)\n", text[i]);
                            i += 1;
                            n += 1;
                        }
                        range_max = n;
                        if (!(tokens[k].mode & RXTOK_MODE_POSSESSIVE))
                            _REWIND_DO_SAVE(k)
                    }
                }
                else
                {
                    just_rewinded = 0;
                    
                    if (tokens[k].mode & RXTOK_MODE_LAZY)
                    {
                        size_t limit = range_max;
                        if (limit == 0)
                            limit = ~limit;
                        
                        if (_REGEX_CHECK_MASK(k, text[i]) && text[i] != 0 && range_min < limit)
                        {
                            printf("match2!! (%c)\n", text[i]);
                            i += 1;
                            range_min += 1;
                            _REWIND_DO_SAVE(k)
                        }
                        else
                        {
                            _REWIND_OR_ABORT()
                            continue;
                        }
                    }
                    else
                    {
                        if (range_max > range_min)
                        {
                            i -= 1;
                            range_max -= 1;
                            _REWIND_DO_SAVE(k)
                        }
                        else
                        {
                            printf("can't rewind greedy match, %zd vs %zd (token %d)\n", range_max, range_min, k);
                            _REWIND_OR_ABORT()
                            continue;
                        }
                    }
                }
            }
            else
            {
                fprintf(stderr, "unimplemented token kind %d\n", tokens[k].kind);
                assert(0);
            }
        }
    }
    
    #undef _REWIND_DO_SAVE
    #undef _REWIND_OR_ABORT
    
    return i;
}

void print_regex_tokens(RegexToken * tokens)
{
    const char * kind_to_str[] = {
        "NORMAL",
        "OPEN",
        "NCOPEN",
        "CLOSE",
        "OR",
        "CARET",
        "DOLLAR",
        "BOUND",
        "NBOUND",
        "END",
    };
    const char * mode_to_str[] = {
        "GREEDY",
        "POSSESS",
        "LAZY",
    };
    for (int k = 0;; k++)
    {
        printf("%s\t%s\t", kind_to_str[tokens[k].kind], mode_to_str[tokens[k].mode]);
        
        int c_old = -1;
        for (int c = 0; c < 256; c++)
        {
            #define _PRINT_C_SMART(c) { \
                if (c >= 0x20 && c <= 0x7E) \
                    printf("%c", c); \
                else \
                    printf("\\x%02x", c); \
            }
                
            if (_REGEX_CHECK_MASK(k, c))
            {
                if (c_old == -1)
                    c_old = c;
            }
            else if (c_old != -1)
            {
                if (c - 1 == c_old)
                {
                    _PRINT_C_SMART(c_old)
                    c_old = -1;
                }
                else if (c - 2 == c_old)
                {
                    _PRINT_C_SMART(c_old)
                    _PRINT_C_SMART(c_old + 1)
                    c_old = -1;
                }
                else
                {
                    _PRINT_C_SMART(c_old)
                    printf("-");
                    _PRINT_C_SMART(c - 1)
                    c_old = -1;
                }
            }
        }
        
        printf("\t");
        for (int i = 0; i < 16; i++)
            printf("%04x", tokens[k].mask[i]);
        
        printf("\t{%d,%d}\t(%d)\n", tokens[k].count_lo, tokens[k].count_hi, tokens[k].pair_offset);
        
        if (tokens[k].kind == RXTOK_KIND_END)
            break;
    }
}

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
    //int e = regex_parse("([0-9]){2,5}?\\.([0-9])+", tokens, &token_count, 0);
    //int e = regex_parse("([0-9]){2,5}\\.([0-9])+", tokens, &token_count, 0);
    //int e = regex_parse("(a|ab)*b", tokens, &token_count, 0);
    //int e = regex_parse("(ab?)*?b", tokens, &token_count, 0);
    int e = regex_parse("(ab?\?)*b", tokens, &token_count, 0);
    //int e = regex_parse("(a)?\?(b|a)", tokens, &token_count, 0);
    
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
    //int64_t match_len = regex_match(tokens, "0120.53) ");
    //int64_t match_len = regex_match(tokens, "1.53) ");
    //int64_t match_len = regex_match(tokens, "aa");
    int64_t match_len = regex_match(tokens, "aaaaaaaaababababb");
    printf("########### return: %zd\n", match_len);
    return 0;
}

#undef _REGEX_CHECK_MASK
