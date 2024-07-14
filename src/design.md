
BBAE (Basic Block Analysis Enabler) Design:

Single BBAE files are analogous to single C translation units. They contain globals, statics (immutable global constants), and functions, and may be linked with other BBAE files or C translation units or anything else; the specifics of linking are entirely implementation-specified.

------

3 kinds of types:

- integers: `i8`, `i16`, `i32`, `i64`, `iptr`
- floats: `f32`, `f64`
- aggregates

The alignment requirements of the first 6 types (i.e. excluding aggregates) are implementation-defined. Integers have no signedness. Floats are binary IEEE 754 floats.

Pointers the same size and can contain the same set of bit patterns as one specific integer type. Which type they're equivalent to is something that depends on the target platform. They are given the unique type iptr, which is an actual type and not just a "typedef".

Aggregate types are defined like:

```rs
{ align.8 i.8 f.4 i.4 }
```

Or:

```rs
{ packed align.8 i.8 f.4 i.4 }
```

i.8 specifies a span of 8 bytes that have integer likeness.

Likeness is only used at function call boundaries. Some ABIs require knowing whether a given chunk of an aggregate contains floats, and if so, how many.

Packedness is only used at function call boundaries. Some ABIs require knowing whether an aggregate type is packed or not.

The alignment directive is required. The alignment value must be a power of 2, and must be 1 or greater.

Two aggregates are the same type if they have the same alignment, same packedness, and each byte is of the same likeness.

```rs
{ align.1 i.2 i.2 }
{ align.1 i.3 i.1 }
```

These are the same type.

The "canonical" form of an aggregate has consecutive runs of the same likeness combined:

```rs
{ align.1 i.4 }
```

Aggregate types have exactly the above syntax and cannot contain specific types (like other aggregate types), only bytes with likeness.

Aggregate types are not named and always occur as their descriptor.

Aggregate types are exactly as long as the number of specified byte likenesses, with no padding.

------

BBAE's grammar is designed to be easy to parse line-by-line. Newlines are significant. Spaces are a universal token boundary, with tabs and spaces being treated identically. The grammar is never recursive or ambiguous. There are no strings or characters. The only literals are integer and float literals. Integer literals can be hex (with a `0x` or `-0x` prefix) or decimal.

A given token cannot be more than 4095 characters long.

BBAE source files are parsed line-by-line. Lines are separated by any number of linefeeds. A linefeed is either `\n` or `\r\n`. Files with mixed linefeed types are allowed.

With only two exceptions, each line is a sequence of tokens, separated by one or more space characters, followed by an end-of-line. A space character is either '` `' (0x20) or `\t`. An end-of-line is either a linefeed, the end of the file, or the beginning of a comment. The beginning of a comment is either `//` or `#`, after which all text is ignored until a linefeed or the end of the file. Lines that contain only a comment are considered to be blank.

There are three types of tokens: symbol tokens, numeric tokens, and text tokens. Numeric tokens are all tokens that are not symbol tokens but begin with a `.` or `-` or digit. A digit is any character out of `1234567890`. Symbol tokens are tokens that exactly match `=`, `{`, `}`, or `<-`. Text tokens are all other tokens.

The two exceptions are lines that begin with an `asm_clobber` or `asm` token. In the `asm_clobber` case, the sequence of characters between the first `<-` and last `<-` on the line are arbitrary text, and are not even subject to comment detection. In the `asm` case, all characters after the `asm` token until the end of the line are considered arbitrary text, and are not even subject to comment detection.

------

Functions are defined like:

```rs
func my_func returns i8
    arg a i8
    arg b { align.8 i.8 }
    return 0i8
endfunc
```

Return type (i.e. the entire `returns <type>` bit) is optional (if missing, function does not return a value). Arguments are optional. Implementations may place a limit on the number of allowed arguments, but it must be at least 127.

Function arguments are only visible to the implicit initial block. To access them from other blocks, they must either be stored in stack slots and loaded later, or passed as block arguments when jumping to the other block.

------

Local stack space for stack variables is defined like:

```rs
func my_func
    stack_slot a 16
endfunc
```

This specifies at least 16 bytes of storage for the stack slot `a`, and creates a SSA variable a, and stores an i64 pointer to that stack slot in that SSA variable. The pointer is assumed to not alias any other memory and cannot be used to access any data outside of its own stack slot. The storage will remain valid exactly until the function exits or the compiler knows that it cannot be legally accessed any more.

Stack slots are analogous to LLVM's alloca.

Stack slot names are visible to all blocks even if the block does not take them as an argument. They evaluate to their pointer, not to their stored value. (This is possible because stack slot access doesn't require doing any register allocation.)

Stack slot directive can only occur at the start of a function, after arguments and before any blocks or statements.

The implicit alignment of stack slots is derived from the size of the stack slot and implementation-defined. The standard alignment is the lowest power of 2 greater than or equal to the stack slot size with a maximum of 64, i.e. a slot of size 16 has 16-byte alignment but a slot of size 17 has 32-byte alignment. This can be overridden to a smaller value with the !align decorator, explained later. Because implicit alignment is implementation-defined, implementations are allowed to use different alignments than the standard alignment.

Stack slots can be reordered, padded, or stored in different places (non-contiguously). For example, `b` in the following example could be padded to 16 bytes, or moved after `a`. It may also, on windows, place the `b` storage in the "shadow space" on the stack belonging to the parent function, while putting the `a` storage in the current function's own stack space, or vice versa. Stack slots may even be allocated on the heap instead of the stack.

```rs
func my_func
    stack_slot b 1
    stack_slot a 16
endfunc
```

The implementation may require and allocate more space for temporaries, or dynamically grow and shrink the stack. The storage size must be an untyped integer literal. Arguments may also, like temporaries, be stored in function-local memory and automatically loaded or stored when accessed.

------

```rs
func my_func
block label
    // statements go here
endfunc
```

Functions consist of a series of basic blocks, each of which only have access to temporary variables declared within themselves or their block arguments.

------

Variables are defined by assigning to them:

```rs
func my_func
block label
    x = 5i8
endfunc
```

Integer literals with no type suffix are assumed to be i64.

------

Variables cannot be reassigned. Function-level local variables are implemented by performing loads and stores on the stack slot pointer:

```rs
func my_func
    stack_slot a 16
block label
    x = load i8 a
    store a 5i8 // could also use x instead of 5i8
endfunc
```

This stores an 8-bit integer with the value '5' into the first byte of stack slot a. In x86_64 assembly, the store instruction would compile down to something like `mov byte ptr [rbp-16], 5` or `mov byte ptr [rsp+0], 5`. The type of a load is determined by a type written between the load instruction and its offset. The type of a store is determined by the type of its second operand.

------

Loads and stores work with aggregates, too:

```rs
func my_func
    stack_slot a 16
block label
    { align.1 i.4 } x = load a
    store a x
endfunc
```

------

Like mentioned earlier, blocks can have arguments. These arguments must be given when jumping to a block:

```rs
func my_func
    // insert other code
block label
    arg x i8
    // statements go here
    goto label2 0i8
block label2
    arg y i8
    // statements go here
endfunc
```

Block fall-through is not allowed, even if the target block has no arguments. The final instruction of a block must be an unconditional goto, or a return, or a program termination. A block cannot contain any instructions past an unconditional goto, or a return, or a program termination.

------

Blocks continue after the else case of conditional gotos. This is different from LLVM, where conditional gotos have true and false target blocks. BBAE blocks, as written in BBAE code, are a limited form of "extended" basic blocks rather than true basic blocks. However, they are designed in a way that makes it trivial to split them into true basic blocks at compile time, enabling basic block analysis without encumbering the user with their limitations.

```rs
func my_func
    // insert other code
block label
    arg x i8
    if x goto label2 15i8
    goto label2 0i8
block label2
    arg y i8
    // statements go here
endfunc
```

------

There are no complex expressions. Only a single operation can be performed in a single statement.

unsupported:

```rs
x * ((x + y) / y)
```

supported:

```rs
x2 = add x y
x3 = div x2 y
x4 = mul x x3
```

------

Function calls apply to an i64 (the function pointer) and have their function signature derived from the argument list and an explicit return type. There are two call instructions, `call` and `call_eval`. `call` is an instruction, and `call_eval` is an operation. call_eval requires a return type. For call, the return type is elided exactly if the function returns void.

```rs
// like f32 x = myfunc(a, b, c)
y = symbol_lookup_unsized myfunc
x = call_eval f32 y a b c
```

------

A statement can either be a temporary variable declaration or an instruction.

A temporary variable declaration looks like:

```rs
<varname> = <operation> <operands>
```

An instruction looks like:

```rs
<instruction> <operands>
```

An operand can only ever be a literal, a variable name, a label name, or a type. Where you're allowed to put which kind of operand is determined by the operation or instruction. Variable names and literals are allowed in exactly the same operand slots.

------

Function declarations consist of a `func ...` directive, followed by zero or more `arg` directives, followed by zero or more `stack_slot` directives, followed by a series of zero or more statements (for the initial block), followed by a series of blocks, followed by an `endfunc` directive.

A block consists of a `block` directive, followed by zero or more `arg` directives, followed one or more statements. The final statement must be a return or an unconditional goto or a program termination.

Potentially-externally-visible instructions (e.g. call, memcpy, store, etc) cannot be reordered relative to each other, nor relative to potentially-externally-visible operations (e.g. call_eval), unless the compiler proves that they are not externally observable (e.g. by inlining a function call, determining that a store goes to non-externally-aliased local memory, etc).

------

Implementation-defined behavior is allowed to produce any arbitrary value, but it must not be poison/undefined. The program is not allowed to be halted or crashed by implementation-defined behavior.

"Implementation-specified" means that the given aspect of the language is entirely implementation-specified, and the implementation may specify undefined behavior.

------

Operations:

```rs
load <type> iptr
ternary i any any // any and any must be of the same type. compiles down to a conditional move, not a branch, except on platforms without conditional moves, unless the compiler determines that a branch would be faster.

add i i // i and i must be of the same type
sub i i // likewise for all of these
mul i i
imul i i

div i i  // division by 0 produces 0
idiv i i // division by 0 produces 0
rem i i  // remainder by 0 produces 0
irem i i // remainder by 0 produces 0

div_unsafe i i  // division by 0 produces implementation-specified behavior
idiv_unsafe i i // division by 0 produces implementation-specified behavior
rem_unsafe i i  // remainder by 0 produces implementation-specified behavior
irem_unsafe i i // remainder by 0 produces implementation-specified behavior

shl i i // shifts i (left) left by i (right) bits
shr i i // shifts right, but at most sizeof(i_left)*8-1 bits, even if i_right stores a larger value. shifts in zero.
shr_unsafe i i // implementation-defined behavior if i_right is sizeof(i_left)*8 or greater
sar i i // likewise, but shifts in the sign bit instead of zero
sar_unsafe i i // likewise

and i i // bitwise and
or i i // bitwise or
xor i i // bitwise xor

cmp_eq i i // produces 1i8 if the values are equal, 0 otherwise. i and i must be of the same type.
cmp_ne i i // !=

cmp_ge i i // >=
cmp_le i i // <=
cmp_g i i // >
cmp_l i i // <

icmp_ge i i // signed >=
icmp_le i i // signed <=
icmp_g i i // signed >
icmp_l i i // signed <

fcmp_eq i i // comparison for floats.
fcmp_ne i i // !=
fcmp_ge i i // >=
fcmp_le i i // <=
fcmp_g i i // >
fcmp_l i i // <

bnot i // flips each bit in the input value and produces a new value of the same type
neg i // produces the arithmetic negative of the input value

not i  // produces 1i8 for 0, 0i8 otherwise.
bool i // produces 0i8 for 0, 1i8 otherwise.

addf f f // f and f must be of the same type
subf f f
mulf f f
divf f f
remf f f

trim <type> i // cast integer to given type, which must be the same size or smaller. bitwise truncation.
zext <type> i // zero-extend integer into the given type, which must be the same size or larger
sext <type> i // sign-extend integer into the given type, which must be the same size or larger

f32_to_f64 f32 // convert float by value
f64_to_f32 f64

float_to_uint <type> f // type must be i8, i16, i32, or i64. out-of-range values are clamped.
float_to_uint_unsafe <type> f // type must be i8, i16, i32, or i64. out-of-range values are implementation-defined behavior.
uint_to_float <type> i // type must be f32 or f64.

float_to_sint <type> f // signed variants of the above
float_to_sint_unsafe <type> f
sint_to_float <type> i

bitcast <type> any // reinterpret the bits of any value as belonging to another type. mainly useful for int <-> float conversions, but can also be used to pun small aggregates as ints/floats, or vice versa, or to convert between different aggregate types of the same size.

extract <type> i agg // extract a value of type `type` starting at byte offset `i` in aggregate `agg`. agg must have a size of at least i + sizeof(type). returns a value of type <type>.
inject any i agg // inject `any` into location at byte offset i in agg. agg must have a size of at least i + sizeof(any). returns a modified copy of agg.

build agg_type any+ // build a value of an aggregate type by packing one or more values together. the values must add up to exactly the same size as the aggregate has. that is the only restriction.

call_eval <type> i any* // zero or more instances of any value as function arguments

// The following operations do not generate any code of their own, but rather affect how optimizations work.

freeze <any> // freezes any poison/undefined data in the given value as fixed but not-yet-known data. analogous to LLVM's freeze. returns the frozen value.

ptralias_inherit iptr iptr // Evaluates to the left iptr, but with the aliasing analysis of the right iptr.
ptralias_merge iptr iptr // Evaluates to the left iptr, but with aliasing analysis that aliases both the left and right iptr.
ptralias_disjoint iptr iptr // Evaluates to the left iptr, starting with the aliasing analysis of the left iptr, but asserting that the output iptr and the right iptr cannot be used to derive eachother or point to each other's data.
ptralias_bless iptr // Evaluates to iptr, but with universal aliasing.
ptralias_curse iptr // Evaluates to iptr, but with no aliasing (i.e. is assumed to point to different data than any prior-existant pointer, including the original iptr).

ptr_to_int iptr // returns an i64 with the same value as the given pointer
int_to_ptr i64 // returns an iptr with the same value as an i64, and universal aliasing
```

------

Instructions:

```rs
goto <label> any* // zero or more instances of any value as block arguments
if i goto <label> any* // zero or more instances of any value as block arguments
return <any>? // return any value, or none if the function doesn't return a value

call <type> i any* // zero or more instances of any value as function arguments

memcpy iptr iptr i // copy i bytes of data pointed to by the right iptr into the location at the left iptr. the two regions may not overlap.
memmove iptr iptr i // copy i bytes of data pointed to by the right iptr into the location at the left iptr. the two regions MAY overlap. however, if the pointers are known to not alias each other, a memcpy may be emitted instead.

store iptr any // stores any value into a pointer. for non-aggregate types, equivalent to a `mov rdi ptr; mov [rdi] reg` instruction sequence.

exit i // exits the program in an implementation-defined way (e.g. can be equivalent to the the C exit(int) function)
interrupt // interrupts the program in an implementation-defined way (e.g. by using the int3 instruction on x86_64)
```

------

For statements, there are five exceptions to the above syntax.

------

The first exception is a pseudo-operation `symbol_lookup_unsized` which returns an iptr but takes an arbitrary string of characters (with no whitespace or newline etc) as its one operand. Normal operations can only do this with label names or variables names. This is what makes this pseudo-operation have exceptional syntax.

```rs
sin = symbol_lookup_unsized sinf
r = call_eval f32 sin 1.1f32
return r
// same as the C code `return sinf(1.1f32);`
```

There is a similar symbol_lookup, which gives the returned iptr an associated aliasing region.

```rs
info_ptr = symbol_lookup info 8
actual_info = load u8 info_ptr
return actual_info
```
Symbol lookup gives priority to symbols defined in the current file. No two symbols in the current file can have the same name.

------

The second exception is a pseudo-instruction `bytes`, which embeds a series of raw byte literals into the instruction stream. It is considered to be a potentially-externally-visible instruction. Byte literals must be untyped, but only the bottom eight bits are considered. Because it only allows untyped literals, not arbitrary values, it is a syntax exception.

```rs
call i8 my_funcptr // assume that my_funcptr can't be inlined or internally analyzed
bytes 0xCC // raise interrupt 3. cannot be reordered to before the `call`, because they're both potentially externally visible.
```

The following, `bytes` with no following bytes, is not legal.

```rs
bytes
```

------

The third exception is the pseudo-instruction `bytes_clobber`, which takes input and output local variables, and corresponding register numbers, as clobbers, separated with a `<-`. The mapping of register numbers to hardware registers, and what types (or even kinds of types) are allowed to be moved into or out of which registers, and what happens when doing so, is implementation-specified. An implementation may even allow moving aggregates into or out of registers, and define what happens when doing so. The register numbers must be integer literals.

```rs
bytes_clobber my_vec2_out 12800 <- 0x66 0x0F 0x51 0xC0 <- my_vec2 12800
```

The above moves the data in my_vec into the register 12800, which in this case we'll assume is xmm0, and then runs the instruction `sqrtpd xmm0,xmm0`, and then moves the register 12800 into a newly-made SSA variable called myvec2_out.

For a multi-input/output example:

```rs
bytes_clobber div_out 0 rem_out 2 <- 0x48 0xF7 0xF2 <- num 0 denom 2
```

The above moves the data in num into register 0, which in we will assume is rax, and denom into register 2, which we will assume is rdx. Then it runs the instruction `div rdx`, which performs the division rax/rdx, then puts the quotient (integer result) in rax and the remainder in rdx. Then it moves rax into the variable div_out and rdx into the variable rem_out.

bytes_clobber instructions can be reordered the same way as any other instruction with their inputs or outputs would be able to be reordered.

Zero-byte bytes_clobber instructions are legal. This can be used to hint to the compiler that a particular variable should be allocated to a particular register at that point in the program, which can affect performance in niche cases on certain architectures (e.g. on certain x86_64 CPUs, decoding instructions with R8-rank registers can be slightly slower than with RAX-rank registers). However, misusing this can result in extra register-to-register moves that hurt performance.

The following is legal and does nothing.

```rs
bytes_clobber <- <-
```

------

The asm instruction, which is optional and implementations do not need to support it, is the same as the bytes instruction, except it takes an arbitrary string of text (which may contain spaces) that is passed to an assembler. The assembly language, assembly flavor, and all other context is implementation-specified, as well as the error cases. Decorators, if any are present, are handled by the implementation, and might not follow the same rules as normal decorators. The given assembly must remain on a single line, but the implementation may specify ways to put mutliple assembly instructions onto a single line.

```rs
asm int3
```

------

The asm_clobber instruction, which is also optional, is the same as the bytes_clobber instruction, except taking assembly instead of raw bytes. The assembly starts at the first `<-` and ends at the last `<-`. The given assembly must remain on a single line, but the implementation may specify ways to put mutliple assembly instructions onto a single line.

```rs
asm_clobber div_out 0 rem_out 2 <- div rdx <- num 0 denom 2
```

------

The following variable names are reserved and always resolve to poison (undefined) values. They should only be used for intentionally creating partially-uninitialized aggregates. If the implementation does not support poison (undefined) values then any arbitrary value is permitted (including different values for different instances of poison).

```rs
psn_i8
psn_i16
psn_i32
psn_i64
psn_iptr
psn_f32
psn_f64
```

------

Statements can have decorators attached. Decorators come at the end of the statement. Decorators may only affect optimization or low-level code generation choices, not the logical behavior of the program.

The following decorators must be supported:

- `!volatile` on loads, stores, and memcpy
- - prevents the operation from being optimized out or reorders against other volatile loads/stores
- `!noinline` on function calls
- - prevents inlining

The following decorators *may* be supported:
- `!align` on stack slots
- - overrides the default computed alignment. compilers that don't support this must choose safe alignments for all stack slot sizes.
- `!abi_sysv` on function calls
- - specifies that the function pointer uses the SysV C ABI.
- `!abi_ms` on function calls
- - specifies that the function pointer uses Microsoft's C ABI.
- `!import_extern` on symbol lookups
- - specifies that the symbol can be acquired from potentially more places than normal, in an implementation-specified manner. intended to reflect the `dllimport` supported by some C compilers.

Unknown decorators are *silently* ignored. However, unknown decorators may indirectly result in an error later during compilation, e.g. if you try accessing an `import_extern` symbol that doesn't exist in anything that the compiler can access, but would be able to access if it supported the decorator.

Arguments and temporary variables may not have the same name as any label in the given function.

Arguments, temporary variables, and labels cannot have the same name as any type. Symbols can.

------

Outside of functions, the following other directives are allowed:

```rs
global <type> <name>
static <type> <name> = i_literal+
```

Static initializers allow only untyped integer literals with a value from 0 to 255.

Examples:

```rs
global i8 pos
global i64 ctx_ptr
static { align.1 i.0x0D } _anon_str = 0x48 0x65 0x6C 0x6C 0x6F 0x2C 0x20 0x77 0x6F 0x72 0x6C 0x64 0x21 0
```

------

Globals and statics are not seen as variables by function code. Rather, they must be looked up as symbols.

Statics are immutable and the compiler will assume that their value does not ever change during execution under any circumstance.

The compiler may generate statics of its own.

------

Function declaration directives, as well as static and global directives, may have the following decorators:

- `!private` - marks the given item as 'private', with implementation-defined consequences. intended to reflect the `static` property of C.
- `!export_extern` - specifies that the symbol can be seen from more places than normal, with implementation-specified consequences. intended to reflect the `dllexport` property of some C compilers.

------

Pointer capture:

In LLVM terms, all pointers passed to all functions are considered captured, unless the compiler can analyze the callee and determine otherwise.

All pointers whose exact values (aside from whether or not they're null) contribute to a value (stored in memory or otherwise) are considered to be captured in that value and any consequent values.

If the compiler loses track of where a given pointer is captured, then any calls to functions that can see state that the callee does not know about must be considered to potentially modify the data pointed to by that pointer, unless the compiler can determine otherwise.

Values with unknown capturing sources (e.g. values passed into a function from external code) are assumed to capture any pointers that have been captured by globals or external code.

In the above, pointers that alias each other are considered captured if at least one of them is captured.

------

Pointer aliasing:

- Pointers of unknown origin are assumed to be able to alias any pointer that they capture or may capture, including all other pointers of unknown origin.
- Pointers derived from `stack_slot` are assumed to alias only pointers derived from the same stack slot.
- Pointers derived from `symbol_lookup_unsized` are assumed to alias any other pointer derived from `symbol_lookup_unsized`, including other symbols.
- Pointers derived from `symbol_lookup` are assumed to alias only other pointers derived from the same symbol. Additionally, only accesses to the defined region can be derived from the pointer. Accesses outside the defined region are UB.
- Pointers derived from anything else are assumed to alias any pointer whose value they derive from in any way, except for null checks.
- Blessed pointers (via ptralias_bless), and all pointers derived from them, are assumed to alias absolutely all pointers.

Things that alias more things override the aliasing considerations of things that alias fewer things. For example, stack_slot-derived pointers alias all pointers of unknown origin, despite being specified as only aliasing pointers from the same stack slot, because pointer of unknown origin alias all pointers.

The compiler is allowed to analyze pointer math and determine statement-specific aliasing accordingly. For example:

```rs
buf = symbol_lookup_unsized buffer
a = add buf 50
b = add buf 250
memmove a b 16
```

The compiler is allowed to know that a and b are only aliased for operations larger than 200 bytes, and convert the memmove into a memcpy because it's only 16 bytes.

------

Special functions:

The special function `_global_init`, if defined, must have zero arguments and no return type. A given file may only have exactly zero or one `_global_init` functions. Linkers must append it to any other static initialization functions (or vice versa) during linking, rather than generating a redefinition error or picking one over the other. It should be run during program startup, at the same time as a C program would run global initializers. This is analogous to LLVM's `@llvm.global_ctors`.
