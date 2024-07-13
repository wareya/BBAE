
Grammar re-specification:

Programs are parsed line-by-line. Lines are tokenized by splitting them at space characters (` ` (0x20) or `\t`). Consecutive space characters do not have empty tokens between them; empty space is elided. All text on a line after the first instance of a comment sigil is ignored, including the comment sigil. The comment sigils are `#` and `//`.

The following token types exist, defined as regexes, and are tested from first (symbol) to last (text), with the first taking priority, stopping at the first space character (` ` (0x20) or `\t`) or the end of the line.

```rs
symbol:  =|<-|{|}
numeric: [\.\-0-9].*
text:    .*
```

The tokenizer is not aware of what symbols look like. Spaces between all tokens are mandatory.

For the sake of simpler grammar specification following meta-token type also exists:

```rs
value: <numeric>|<text>
```

Types occasionally occur within a given line. Types match any of the following:

```rs
i8
i16
i32
i64
f32
f64
<symbol> <text>+ <symbol>
```

Above and below, names within <> refer to a previously-defined type of token.

The last type syntax rule is an aggregate type. Aggregates have their own internal rules, but each internal token matches as a "text" token.

The tokens making up an aggregate type must follow the format:

```rs
{ (packed)? align\.[0-9]+ ((i|f)\.[0-9]+)+ }
```

For example:

```rs
{ packed align.8 f.4 i.4 }
```

Any symbol pair other than `{` and `}` around the aggregate type is a syntax error. `packed` present anywhere other than the first text token is a syntax error. `align.N` present anywhere other than the first non-`packed` text token is a syntax error. Subsequent text tokens of any format other than `(i|f).N` are a syntax error. Values of `0` for the `N` after `align.`, `i.`, or `f.` are a semantic error.

For statements, the following operations are supported:

```rs
load <type> <value>
ternary <value> <value> <value>
(add|sub|mul|imul|div|idiv|rem|irem|div_unsafe|idiv_unsafe|rem_unsafe|irem_unsafe) <value> <value>
(shl|shr|shr_unsafe|sar|sar_unsafe|and|or|xor) <value> <value>
cmp_(eq|ne|ge|le|g|l) <value> <value>
icmp_(ge|le|g|l) <value> <value>
fcmp_(eq|ne|ge|le|g|l) <value> <value>
(bnot|not|bool) <value>
(addf|subf|mulf|divf|remf) <value> <value>
(trim|zext|sext) <type> <value>
(f32_to_f64|f64_to_f32) <value>
(float_to_uint|float_to_uint_unsafe|uint_to_float) <type> <value>
(float_to_sint|float_to_sint_unsafe|sint_to_float) <type> <value>
bitcast <type> <value>
extract <type> <value> <value>
inject <value> <value> <value>
build <type> <value>+ // type must be an aggregate
call_eval <type> <value> <value>*
freeze <value>
(ptralias|ptralias_merge|ptralias_disjoint) <value> <value>
ptralias_bless <value>
symbol_lookup_unsized <text> // arbitrary text, does not need to be a valid label, but cannot contain spaces or newlines or // or #
symbol_lookup <text> <numeric>
```

Operations are used in the form:

```rs
<text> = <operation>
```

Instructions:

```rs
goto <text> <value>*
if <value> goto <text> <value>*
return <value>
call <type> <value> <value>*
(memcpy|memmove) <value> <value> <value>
store <value> <value>
exit <value>
interrupt
```

The exceptional instructions `bytes`, `bytes_clobber`, `asm`, and `asm_clobber` were defined earlier in this document. A grammar-like description of them follows.

```rs
bytes <untyped integer literal>+
bytes_clobber (<text> <untyped integer literal>)* <- <untyped integer literal>* <- (<text> <untyped integer literal>)*
asm <arbitrary text>
bytes_clobber (<text> <untyped integer literal>)* <- <arbitrary text> <- (<text> <untyped integer literal>)*
```

Outside of statements, the following directives are possible:

```rs
func <text> (returns <type>)?
arg <text> <type>
stack_slot <text> <type>
block <text>
endfunc
global <type> <text>
static <type> <text> = <untyped integer literal>+
```

Directives are only allowed in specific places. `func`, `global`, and `static` directives can only happen in the 'root' of the file. `arg` can only happen in a list of args after a `func` or `block`. 

Statements and directives may be followed by decorators, which begin with `!`. Once a decorator begins, only decorators and end-of-line (including comments) are allowed. Decorators must be separated by any nonzero number of spaces, and also separated from any tokens.
