#ifndef INCLUDE_RXTOK

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

static const int RXTOK_FLAG_DOT_NEWLINES = 1;

static const uint8_t RXTOK_KIND_NORMAL      = 0;
static const uint8_t RXTOK_KIND_OPEN        = 1;
static const uint8_t RXTOK_KIND_NCOPEN      = 2;
static const uint8_t RXTOK_KIND_CLOSE       = 3;
static const uint8_t RXTOK_KIND_OR          = 4;
static const uint8_t RXTOK_KIND_CARET       = 5;
static const uint8_t RXTOK_KIND_DOLLAR      = 6;
static const uint8_t RXTOK_KIND_BOUND       = 7;
static const uint8_t RXTOK_KIND_NBOUND      = 8;
static const uint8_t RXTOK_KIND_END         = 9;

static const uint8_t RXTOK_MODE_POSSESSIVE  = 1;
static const uint8_t RXTOK_MODE_LAZY        = 2;
static const uint8_t RXTOK_MODE_INVERTED    = 128; // temporary; gets cleared later

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
    uint64_t pattern_len = strlen(pattern);
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
    
    #define _REGEX_CLEAR_TOKEN(TOKEN) { memset(&(TOKEN), 0, sizeof(RegexToken)); token.count_lo = 1; token.count_hi = 2; }
    
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
    
    for (uint64_t i = 0; i < pattern_len; i++)
    {
        char c = pattern[i];
        if (state == STATE_QUANT)
        {
            state = STATE_MODE;
            if (c == '?')
            {
                token.count_lo = 0;
                token.count_hi = 2; // first non-allowed amount
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
                    token.count_hi = val + 1;
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
                            token.count_hi = val2 + 1;
                        }
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
                    token.count_hi = 1;
                    if (pattern[i + 1] == '?' && pattern[i + 2] == ':')
                    {
                        token.kind = RXTOK_KIND_NCOPEN;
                        i += 2;
                    }
                    else if (pattern[i + 1] == '?' && pattern[i + 2] == '>')
                    {
                        token.kind = RXTOK_KIND_NCOPEN;
                        _REGEX_PUSH_TOKEN()
                        
                        state = STATE_NORMAL;
                        token.kind = RXTOK_KIND_NCOPEN;
                        token.mode = RXTOK_MODE_POSSESSIVE;
                        token.count_lo = 1;
                        token.count_hi = 2;
                        
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
                    // phantom group for atomic group emulation
                    if (tokens[found].mode == RXTOK_MODE_POSSESSIVE)
                    {
                        _REGEX_PUSH_TOKEN()
                        token.kind = RXTOK_KIND_CLOSE;
                        token.mode = RXTOK_MODE_POSSESSIVE;
                        token.pair_offset = -diff - 2;
                        tokens[found - 1].pair_offset = diff + 2;
                    }
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
                    
                    // find corresponding ( and how far away it is. store in token
                    int balance = 0;
                    ptrdiff_t found = -1;
                    for (ptrdiff_t l = k - 1; l >= 0; l--)
                    {
                        if (tokens[l].kind == RXTOK_KIND_NCOPEN || tokens[l].kind == RXTOK_KIND_OPEN)
                        {
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
    token.count_hi = 2;
    _REGEX_PUSH_TOKEN();
    
    // add end token (tells matcher that it's done)
    token.kind = RXTOK_KIND_END;
    _REGEX_PUSH_TOKEN();
    
    tokens[0].pair_offset = k - 2;
    tokens[k-2].pair_offset = -(k - 2);
    
    *token_count = k;
    
    // copy quantifiers from )s to (s (so (s know whether they're optional)
    // also take the opportunity to smuggle "quantified group index" into the mask field for the )
    uint64_t n = 0;
    for (int16_t k2 = 0; k2 < k; k2++)
    {
        if (tokens[k2].kind == RXTOK_KIND_CLOSE)
        {
            tokens[k2].mask[0] = n++;
            
            int16_t k3 = k2 + tokens[k2].pair_offset;
            tokens[k3].count_lo = tokens[k2].count_lo;
            tokens[k3].count_hi = tokens[k2].count_hi;
            tokens[k3].mask[0] = n++;
            tokens[k3].mode = tokens[k2].mode;
            
            //if (n > 65535)
            if (n > 2048)
                return -1; // too many quantified groups
        }
    }
    
    #undef _REGEX_PUSH_TOKEN
    #undef _REGEX_SET_MASK
    #undef _REGEX_CLEAR_TOKEN
    
    return 0;
}

typedef struct _RegexMatcherState {
    uint32_t k;
    uint32_t group_state; // quantified group temp state (e.g. number of repetitions)
    uint32_t prev; // for )s, stack index of corresponding previous quantified state
#ifdef REGEX_STACK_SMOL
    uint32_t i;
    uint32_t range_min;
    uint32_t range_max;
#else
    uint64_t i;
    uint64_t range_min;
    uint64_t range_max;
#endif
} RegexMatcherState;

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
    
#ifdef REGEX_VERBOSE
    const uint8_t verbose = 1;
#else
    const uint8_t verbose = 0;
#endif
    
#define IF_VERBOSE(X) { if (verbose) { X } }
    
#ifdef REGEX_STACK_SMOL
    const uint16_t stack_size_max = 256;
#else
    const uint16_t stack_size_max = 1024;
#endif
    const uint16_t aux_stats_size = 1024;
    
    uint64_t tokens_len = 0;
    uint32_t k = 0;
    
    // quantified group state
    uint8_t q_group_accepts_zero[aux_stats_size];
    uint32_t q_group_state[aux_stats_size]; // number of repetitions
    uint32_t q_group_stack[aux_stats_size]; // location of most recent corresponding ) on stack. 0 means nowhere
    while (tokens[k].kind != RXTOK_KIND_END)
    {
        k += 1;
        if (tokens[k].kind == RXTOK_KIND_CLOSE || tokens[k].kind == RXTOK_KIND_OPEN || tokens[k].kind == RXTOK_KIND_NCOPEN)
        {
            if (tokens[k].mask[0] >= aux_stats_size)
            {
                puts("too many qualified groups. returning");
                return -2; // OOM: too many quantified groups
            }
            
            q_group_state[tokens[k].mask[0]] = 0;
            q_group_stack[tokens[k].mask[0]] = 0;
            q_group_accepts_zero[tokens[k].mask[0]] = 0;
        }
    }
    
    tokens_len = k;
    
    RegexMatcherState rewind_stack[stack_size_max];
    uint16_t stack_n = 0;
    
    uint64_t i = 0;
    
    uint64_t range_min = 0;
    uint64_t range_max = 0;
    uint8_t just_rewinded = 0;
    
    #define _P_TEXT_HIGHLIGHTED() { \
        IF_VERBOSE(printf("\033[91m"); \
        for (uint64_t q = 0; q < i; q++) printf("%c", text[q]); \
        printf("\033[0m"); \
        for (uint64_t q = i; text[q] != 0; q++) printf("%c", text[q]); \
        printf("\n");) \
    }
    
    #define _REWIND_DO_SAVE(K) { \
        if (stack_n >= stack_size_max) \
        { \
            puts("out of backtracking room. returning"); \
            return -2; \
        } \
        RegexMatcherState s; \
        s.i = i; \
        s.k = (K); \
        s.range_min = range_min; \
        s.range_max = range_max; \
        s.prev = 0; \
        if (tokens[s.k].kind == RXTOK_KIND_CLOSE) \
        { \
            s.group_state = q_group_state[tokens[s.k].mask[0]]; \
            s.prev = q_group_stack[tokens[s.k].mask[0]]; \
            q_group_stack[tokens[s.k].mask[0]] = stack_n; \
        } \
        rewind_stack[stack_n++] = s; \
        _P_TEXT_HIGHLIGHTED() \
        IF_VERBOSE(printf("-- saving rewind state k %u i %zd rmin %zu rmax %zd (line %d) (depth %d prev %d)\n", s.k, i, range_min, range_max, __LINE__, stack_n, s.prev);) \
    }
    
    #define _REWIND_OR_ABORT() { \
        if (stack_n == 0) \
            return -1; \
        stack_n -= 1; \
        just_rewinded = 1; \
        range_min = rewind_stack[stack_n].range_min; \
        range_max = rewind_stack[stack_n].range_max; \
        assert(rewind_stack[stack_n].i <= i); \
        i = rewind_stack[stack_n].i; \
        k = rewind_stack[stack_n].k; \
        if (tokens[k].kind == RXTOK_KIND_CLOSE) \
        { \
            q_group_state[tokens[k].mask[0]] = rewind_stack[stack_n].group_state; \
            q_group_stack[tokens[k].mask[0]] = rewind_stack[stack_n].prev; \
        } \
        _P_TEXT_HIGHLIGHTED() \
        IF_VERBOSE(printf("-- rewound to k %u i %zd rmin %zu rmax %zd (kind %d prev %d)\n", k, i, range_min, range_max, tokens[k].kind, rewind_stack[stack_n].prev);) \
        k -= 1; \
    }
    // the -= 1 is because of the k++ in the for loop
    
    size_t limit = 10000;
    for (k = 0; k < tokens_len; k++)
    {
        //limit--;
        if (limit == 0)
        {
            puts("iteration limit exceeded. returning");
            return -2;
        }
        IF_VERBOSE(printf("k: %u\ti: %zu\tl: %zu\tstack_n: %d\n", k, i, limit, stack_n);)
        _P_TEXT_HIGHLIGHTED();
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
            if (tokens[k].count_hi == 1)
            {
                if (tokens[k].kind == RXTOK_KIND_OPEN || tokens[k].kind == RXTOK_KIND_NCOPEN)
                    k += tokens[k].pair_offset;
                else
                    k += 1;
                continue;
            }
            
            if (tokens[k].kind == RXTOK_KIND_OPEN || tokens[k].kind == RXTOK_KIND_NCOPEN)
            {
                if (!just_rewinded)
                {
                    IF_VERBOSE(printf("hit OPEN. i is %zd, depth is %d\n", i, stack_n);)
                    // need this to be able to detect and reject zero-size matches
                    //q_group_state[tokens[k].mask[0]] = i;
                    
                    // if we're lazy and the min length is 0, we need to try the non-group case first
                    if ((tokens[k].mode & RXTOK_MODE_LAZY) && (tokens[k].count_lo == 0 || q_group_accepts_zero[tokens[k + tokens[k].pair_offset].mask[0]]))
                    {
                        puts("trying non-group case first.....");
                        range_min = 0;
                        range_max = 0;
                        _REWIND_DO_SAVE(k)
                        k += tokens[k].pair_offset; // automatic += 1 will put us past the matching )
                    }
                    else
                    {
                        range_min = 1;
                        range_max = 0;
                        _REWIND_DO_SAVE(k)
                    }
                }
                else
                {
                    IF_VERBOSE(printf("rewinded into OPEN. i is %zd, depth is %d\n", i, stack_n);)
                    just_rewinded = 0;
                    
                    uint64_t orig_k = k;
                    
                    IF_VERBOSE(printf("--- trying to try another alternation, start k is %d, rmin is %zu\n", k, range_min);)
                    
                    if (range_min != 0)
                    {
                        IF_VERBOSE(puts("rangemin is not zero. checking...");)
                        k += range_min;
                        IF_VERBOSE(printf("start kind: %d\n", tokens[k].kind);)
                        int32_t pb = 0;
                        while (tokens[k].kind != RXTOK_KIND_END)
                        {
                            if (pb == 0 && (tokens[k].kind == RXTOK_KIND_CLOSE || tokens[k].kind == RXTOK_KIND_OR))
                                break;
                            if (tokens[k].kind == RXTOK_KIND_OPEN || tokens[k].kind == RXTOK_KIND_NCOPEN)
                                pb += 1;
                            else if (tokens[k].kind == RXTOK_KIND_CLOSE)
                                pb -= 1;
                            IF_VERBOSE(printf("....%d += 1 (%d)\n", k, pb);)
                            
                            k += 1;
                        }
                        IF_VERBOSE(printf("kamakama %d %d\n", k, tokens[k].kind);)
                        
                        if (tokens[k].kind == RXTOK_KIND_END) // unbalanced parens
                            return -3;
                        
                        IF_VERBOSE(printf("---?!?!   %d, %d\n", k, q_group_state[tokens[k].mask[0]]);)
                        if (tokens[k].kind == RXTOK_KIND_CLOSE)
                        {
                            IF_VERBOSE(puts("!!~!~!~~~~!!~~!~   hit CLOSE. rewinding");)
                            /*
                            _REWIND_OR_ABORT()
                            continue;
                            */
                            // do nothing and continue on if we don't need this group
                            if (tokens[k].count_lo == 0 || q_group_accepts_zero[tokens[k].mask[0]])
                            {
                                IF_VERBOSE(puts("continuing because we don't need this group");)
                                continue;
                            }
                            // otherwise go to the last point before the group
                            else
                            {
                                IF_VERBOSE(puts("going to last point before this group");)
                                _REWIND_OR_ABORT()
                                continue;
                            }
                        }
                        
                        assert(tokens[k].kind == RXTOK_KIND_OR);
                    }
                    
                    ptrdiff_t k_diff = k - orig_k;
                    range_min = k_diff + 1;
                    
                    IF_VERBOSE(puts("(saving in paren after rewinding and looking for next regex token to check)");)
                    IF_VERBOSE(printf("%zd\n", range_min);)
                    _REWIND_DO_SAVE(k - k_diff)
                }
            }
            else if (tokens[k].kind == RXTOK_KIND_CLOSE)
            {
                if (tokens[k].count_lo != 1 || tokens[k].count_hi != 2)
                {
                    IF_VERBOSE(puts("closer test.....");)
                    if (!just_rewinded)
                    {
                        uint32_t prev = q_group_stack[tokens[k].mask[0]];
                        
                        IF_VERBOSE(printf("qrqrqrqrqrqrqrq-------      k %d, gs %d, gaz %d, i %zd, tklo %d, rmin %zd, tkhi %d, rmax %zd, prev %d\n", k, q_group_state[tokens[k].mask[0]], q_group_accepts_zero[tokens[k].mask[0]], i, tokens[k].count_lo, range_min, tokens[k].count_hi, range_max, prev);)
                        
                        range_max = tokens[k].count_hi;
                        range_max -= 1;
                        range_min = q_group_accepts_zero[tokens[k].mask[0]] ? 0 : tokens[k].count_lo;
                        //assert(q_group_state[tokens[k + tokens[k].pair_offset].mask[0]] <= i);
                        //if (prev) assert(rewind_stack[prev].i <= i);
                        IF_VERBOSE(printf("qzqzqzqzqzqzqzq-------      rmin %zd, rmax %zd\n", range_min, range_max);)
                        
                        uint8_t force_zero = 0;
                        if ((tokens[k].mode & RXTOK_MODE_LAZY) && prev != 0 && (uint32_t)rewind_stack[prev].i > (uint32_t)i)
                        {
                            // find matching open paren
                            size_t n = stack_n - 1;
                            while (n > 0 && rewind_stack[n].k != k + tokens[k].pair_offset)
                                n -= 1;
                            assert(n > 0);
                            if (rewind_stack[n].i == i)
                                force_zero = 1;
                        }
                        
                        // minimum requirement not yet met
                        if (q_group_state[tokens[k].mask[0]] + 1 < range_min && !q_group_accepts_zero[tokens[k].mask[0]])
                        {
                            IF_VERBOSE(puts("???????????????     bruh????");)
                            q_group_state[tokens[k].mask[0]] += 1;
                            _REWIND_DO_SAVE(k)
                            k += tokens[k].pair_offset; // back to start of group
                            k -= 1; // ensure we actually hit the group node next and not the node after it
                        }
                        // maximum allowance exceeded
                        else if (tokens[k].count_hi != 0 && q_group_state[tokens[k].mask[0]] + 1 > range_max && !q_group_accepts_zero[tokens[k].mask[0]])
                        {
                            IF_VERBOSE(printf("!!!!!!!!!!!     max???? %d %zd\n", q_group_state[tokens[k].mask[0]], range_max);)
                            range_max -= 1;
                            _REWIND_OR_ABORT()
                        }
                        // reject zero-length matches
                        else if (force_zero || (prev != 0 && (uint32_t)rewind_stack[prev].i == (uint32_t)i)) //  && q_group_state[tokens[k].mask[0]] > 0
                        {
                            IF_VERBOSE(puts("rejecting zero-length match.....");)
                            IF_VERBOSE(printf("%d (k: %d)\n", q_group_state[tokens[k].mask[0]], k);)
                            //range_max = q_group_state[tokens[k].mask[0]];
                            q_group_accepts_zero[tokens[k].mask[0]] = 1;
                            _REWIND_OR_ABORT()
                            //range_max = q_group_state[tokens[k].mask[0]];
                            //range_min = 0;
                        }
                        else if (tokens[k].mode & RXTOK_MODE_LAZY) // lazy
                        {
                            IF_VERBOSE(printf("nidnfasidfnidfndifn-------      %d, %d, %zd\n", q_group_state[tokens[k].mask[0]], tokens[k].count_lo, range_min);)
                            if (prev)
                                printf("lazy doesn't think it's zero-length. prev i %zd vs i %zd (depth %d)\n", rewind_stack[prev].i, i, stack_n);
                            // continue on to past the group; group retry is in rewind state
                            q_group_state[tokens[k].mask[0]] += 1;
                            _REWIND_DO_SAVE(k)
                            q_group_state[tokens[k].mask[0]] = 0;
                            //q_group_stack[tokens[k].mask[0]] = 0;
                        }
                        else // greedy
                        {
                            IF_VERBOSE(puts("wahiwahi");)
                            // clear unwanted memory if possessive
                            if ((tokens[k].mode & RXTOK_MODE_POSSESSIVE))
                            {
                                uint32_t k2 = k;
                                
                                // special case for first, only rewind to (, not to )
                                if (q_group_state[tokens[k].mask[0]] == 0)
                                {
                                    k2 = k + tokens[k].pair_offset;
                                    if (stack_n == 0)
                                        return -1;
                                    stack_n -= 1;
                                }
                                
                                while (stack_n > 0 && rewind_stack[stack_n].k != k2)
                                    stack_n -= 1;
                                if (stack_n == 0)
                                    return -1;
                            }
                            // continue to next match if sane
                            if ((uint32_t)q_group_state[tokens[k + tokens[k].pair_offset].mask[0]] < (uint32_t)i)
                            {
                                q_group_state[tokens[k].mask[0]] += 1;
                                _REWIND_DO_SAVE(k)
                                k += tokens[k].pair_offset; // back to start of group
                                k -= 1; // ensure we actually hit the group node next and not the node after it
                            }
                        }
                    }
                    else
                    {
                        IF_VERBOSE(puts("IN CLOSER REWIND!!!");)
                        just_rewinded = 0;
                        
                        if (tokens[k].mode & RXTOK_MODE_LAZY)
                        {
                            // lazy rewind: need to try matching the group again
                            q_group_stack[tokens[k].mask[0]] = stack_n;
                            k += tokens[k].pair_offset; // back to start of group
                            k -= 1; // ensure we actually hit the group node next and not the node after it
                        }
                        else
                        {
                            // greedy. if we're going to go outside the acceptable range, rewind
                            IF_VERBOSE(printf("kufukufu %d %zd\n", tokens[k].count_lo, range_min);)
                            //uint64_t old_i = i;
                            if (q_group_state[tokens[k].mask[0]] < range_min)
                            {
                                IF_VERBOSE(puts("rewinding from greedy group");)
                                //i = old_i;
                                _REWIND_OR_ABORT()
                            }
                            // otherwise continue on to past the group
                            else
                            {
                                IF_VERBOSE(puts("continuing past greedy group");)
                                q_group_state[tokens[k].mask[0]] = 0;
                                //q_group_stack[tokens[k].mask[0]] = 0;
                            }
                        }
                    }
                }
            }
            else if (tokens[k].kind == RXTOK_KIND_OR)
            {
                //assert(((void)"BUG", !just_rewinded));
                while (tokens[k].kind != RXTOK_KIND_END && tokens[k].kind != RXTOK_KIND_CLOSE)
                    k += 1;
                k -= 1;
            }
            else if (tokens[k].kind == RXTOK_KIND_NORMAL)
            {
                //puts("a");
                if (!just_rewinded)
                {
                    uint64_t n = 0;
                    // do whatever the obligatory minimum amount of matching is
                    uint64_t old_i = i;
                    while (n < tokens[k].count_lo && _REGEX_CHECK_MASK(k, text[i]))
                    {
                        //printf("match?? (%c) (token %d)\n", text[i], k);
                        i += 1;
                        n += 1;
                    }
                    if (n < tokens[k].count_lo)
                    {
                        IF_VERBOSE(printf("non-match A. rewinding (token %d)\n", k);)
                        i = old_i;
                        _REWIND_OR_ABORT()
                        continue;
                    }
                    
                    if (tokens[k].mode & RXTOK_MODE_LAZY)
                    {
                        range_min = n;
                        range_max = tokens[k].count_hi - 1;
                        _REWIND_DO_SAVE(k)
                    }
                    else
                    {
                        uint64_t limit = tokens[k].count_hi;
                        if (limit == 0)
                            limit = ~limit;
                        range_min = n;
                        while (_REGEX_CHECK_MASK(k, text[i]) && text[i] != 0 && n + 1 < limit)
                        {
                            IF_VERBOSE(printf("match!! (%c)\n", text[i]);)
                            i += 1;
                            n += 1;
                        }
                        range_max = n;
                        IF_VERBOSE(printf("set rmin to %zd and rmax to %zd on entry into normal greedy token with k %d\n", range_min, range_max, k);)
                        if (!(tokens[k].mode & RXTOK_MODE_POSSESSIVE))
                            _REWIND_DO_SAVE(k)
                    }
                }
                else
                {
                    just_rewinded = 0;
                    
                    if (tokens[k].mode & RXTOK_MODE_LAZY)
                    {
                        uint64_t limit = range_max;
                        if (limit == 0)
                            limit = ~limit;
                        
                        if (_REGEX_CHECK_MASK(k, text[i]) && text[i] != 0 && range_min < limit)
                        {
                            IF_VERBOSE(printf("match2!! (%c) (k: %d)\n", text[i], k);)
                            i += 1;
                            range_min += 1;
                            _REWIND_DO_SAVE(k)
                        }
                        else
                        {
                            IF_VERBOSE(printf("core rewind lazy (k: %d)\n", k);)
                            _REWIND_OR_ABORT()
                        }
                    }
                    else
                    {
                        //IF_VERBOSE(printf("comparing rmin %zd and rmax %zd token with k %d\n", range_min, range_max, k);)
                        /*if (range_max == range_min)
                        {
                            IF_VERBOSE(puts("CONTINUING!!!!!");)
                            ; // do nothing, continue to next regex token without this rewind state
                        }
                        else */if (range_max > range_min)
                        {
                            IF_VERBOSE(printf("greedy normal going back (k: %d)\n", k);)
                            i -= 1;
                            range_max -= 1;
                            _REWIND_DO_SAVE(k)
                        }
                        else
                        {
                            IF_VERBOSE(printf("core rewind greedy (k: %d)\n", k);)
                            _REWIND_OR_ABORT()
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
        //printf("k... %d\n", k);
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
        for (int c = 0; c < (tokens[k].kind ? 0 : 256); c++)
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
        
        printf("\t{%d,%d}\t(%d)\n", tokens[k].count_lo, tokens[k].count_hi - 1, tokens[k].pair_offset);
        
        if (tokens[k].kind == RXTOK_KIND_END)
            break;
    }
}

#undef _REGEX_CHECK_MASK

#endif //INCLUDE_RXTOK
