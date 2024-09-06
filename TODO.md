# TODO

- finish implementing basic operations/instructions
- structs
- emit object code

## Operation checklist

```rs
[x] mov any // copy any value into a new SSA variable

[x] load <type> iptr
[ ] ternary i any any // if i is nonzero, evaluates to left any, otherwise evaluates to right any. any and any must be of the same type. compiles down to a conditional move, not a branch, except on platforms without conditional moves, unless the compiler determines that a branch would be faster.

[x] add i i // i and i must be of the same type, OR, left is iptr and right is native-pointer-sized int (i64 or i32)
[x] sub i i // likewise for all of these
[x] mul i i // unsigned multiplication
[x] imul i i // signed multiplication

[?] div i i  // division by 0 produces 1
[?] idiv i i
[?] rem i i // remainder by 0 produces 0
[?] irem i i

[ ] div_unsafe i i  // division by 0 produces implementation-specified behavior
[ ] idiv_unsafe i i // likewise
[ ] rem_unsafe i i  // likewise
[ ] irem_unsafe i i // likewise

[?] shl i i // shifts i (left) left by i (right) bits. shifts in zero.
[?] shr i i // shifts right. if sizeof(type)*8 or more bits are shifted, the result is 0.
[ ] shr_unsafe i i // implementation-defined behavior on bit width overflow
[ ] sar i i // likewise, but shifts in the sign bit instead of zero.
[ ] sar_unsafe i i // implementation-defined behavior on bit width overflow

[ ] and i i // bitwise and
[ ] or i i  // bitwise or
[ ] xor i i // bitwise xor
[ ] bitnot i // flips each bit in the input value and produces a new value of the same type

[ ] neg i    // produces the arithmetic negative of the input value (positive overflow is defined behavior -- neg 0x8000u16 produces 0x8000u16)
[ ] bool i   // produces 0i8 for any integer 0 value (including iptr), 1i8 otherwise.
[ ] not i    // produces 1i8 for any integer 0 value (including iptr), 0i8 otherwise.

[ ] cmp_eq i i // produces 1i8 if the values are equal, 0 otherwise. i and i must be of the same type.
[ ] cmp_ne i i // !=

[ ] cmp_ge i i // unsigned >=
[ ] cmp_le i i // unsigned <=
[x] cmp_g i i // unsigned >
[ ] cmp_l i i // unsigned <

[ ] icmp_ge i i // signed >=
[ ] icmp_le i i // signed <=
[ ] icmp_g i i // signed >
[ ] icmp_l i i // signed <

[ ] fcmp_eq f f // == comparison for floats.
[ ] fcmp_ne f f // !=
[ ] fcmp_ge f f // >=
[ ] fcmp_le f f // <=
[ ] fcmp_g f f // >
[ ] fcmp_l f f // <

[x] fadd f f // f and f must be of the same type
[x] fsub f f
[x] fmul f f
[x] fdiv f f

[ ] fneg f // sign-negates a float (e.g. x * -1.0), producing a float of the same type. behavior for NaN inputs is implementation-defined.
[ ] fbool f // produces 0u8 if the float is 0.0 or -0.0, and 1u8 otherwise (including producing 1u8 for NaN)
[ ] fisnan f // produces 1u8 if the float is NaN, and 0u8 otherwise

[ ] trim <type> i // cast integer to given type, which must be the same size or smaller. bitwise truncation, trims away any higher bits.
[ ] qext <type> i // quick extend integer into the given type, which must be the same size or larger. any new bits contain arbitrary data. (this arbitrary data is not undefined and not poison.)
[ ] zext <type> i // zero-extend integer into the given type, which must be the same size or larger. any new bits contain zero.
[ ] sext <type> i // sign-extend integer into the given type, which must be the same size or larger. any new bits contain a copy of the uppermost old bit.
// the above four instructions are no-ops if the input and output sizes are the same. valid types are i8, i16, i32, and i64. iptr is not supported.

[ ] f32_to_f64 f32 // convert float by value
[ ] f64_to_f32 f64

[ ] float_to_uint <type> f // type must be i8, i16, i32, or i64. out-of-range values are clamped.
[ ] float_to_uint_unsafe <type> f // type must be i8, i16, i32, or i64. out-of-range values are implementation-defined behavior.
[x] uint_to_float <type> i // type must be f32 or f64.

[ ] float_to_sint <type> f // signed variants of the above
[ ] float_to_sint_unsafe <type> f
[ ] sint_to_float <type> i

[ ] bitcast <type> any // reinterpret the bits of any value as belonging to another type of the same size. mainly useful for int <-> float conversions, but can also be used to pun small aggregates as ints/floats, or vice versa, or to convert between different aggregate types of the same size.

[ ] extract <type> i agg // extract a value of type `type` starting at byte offset `i` in aggregate `agg`. agg must have a size of at least i + sizeof(type). returns a value of type <type>.
[ ] inject any i agg // inject `any` into location at byte offset i in agg. agg must have a size of at least i + sizeof(any). returns a modified copy of agg.

[ ] build agg_type any+ // build a value of an aggregate type by packing one or more values together. the values must add up to exactly the same size as the aggregate has. that is the only restriction.

[ ] call_eval <type> iptr any* // zero or more instances of any value as function arguments. iptr is function pointer. <type> is return type

[ ] symbol_lookup <raw text> <int literal> // does implementation-defined symbol lookup, returning an iptr (must be able to find functions, globals and static defined in the current module)
[ ] symbol_lookup_unsized <raw text> // same, but without an aliasing region size

[ ] freeze <any> // freezes any poison/undefined data in the given value as fixed but not-yet-known data. analogous to LLVM's freeze. returns the frozen value.

[ ] ptralias_reinterpret iptr iptr // Evaluates to the left iptr, but with the aliasing analysis of the right iptr.
[ ] ptralias_merge iptr iptr // Evaluates to the left iptr, but with aliasing analysis that aliases both the left and right iptr.
[ ] ptralias_disjoint iptr iptr // Evaluates to the left iptr, starting with the aliasing analysis of the left iptr, but asserting that the output iptr and the right iptr cannot be used to derive eachother or point to each other's data. does not assert the same for the two input pointers.
[ ] ptralias_bless iptr // Evaluates to iptr, but with universal aliasing.
[ ] ptralias_curse iptr // Evaluates to iptr, but with no aliasing (i.e. is assumed to point to different data than any prior-existant pointer, **including the original iptr**).

[ ] ptr_to_int iptr // returns an int of the native pointer size with the same value as the given pointer.
[ ] int_to_ptr_blessed i // returns an iptr with the same value as an integer, and universal aliasing. primarily used for int-punned pointers. int must be of the native pointer size.
[ ] int_to_ptr_cursed i // returns an iptr with the same value as an integer, and no aliasing. primarily used for offsets. int must be of the native pointer size.
```

## Instruction checklist

```rs
[x] goto <label> any* // zero or more instances of any value as block arguments
[x] if i goto <label> any* // zero or more instances of any value as block arguments
[x] return <any>? // return any value, or none if the function doesn't return a value

[x] call <type> iptr any* // zero or more instances of any value as function arguments. iptr is function pointer. <type> is return type. return type is specified because it might require extra work, depending on the exact type and the ABI. for example, most ABIs require large returned structs to be passed in as a zeroth pointer argument.

[ ] memcpy iptr iptr i // copy i bytes of data pointed to by the right iptr into the location at the left iptr. the two regions may not overlap.
[ ] memmove iptr iptr i // copy i bytes of data pointed to by the right iptr into the location at the left iptr. the two regions MAY overlap. however, if the pointers are known to not alias each other, the code for memcpy may be emitted instead.

[x] store iptr any // stores any value into a pointer. for non-aggregate types, equivalent to a `mov rdi ptr; mov [rdi] reg` instruction sequence.

[ ] exit i // gracefully exits the program in an implementation-defined way (e.g. can be equivalent to the the C exit(int) function). If equivalent to the C exit(int) function, must give a best-effort attempt at passing the given integer value.
[ ] interrupt // ungracefully halts/interrupts the program in an implementation-defined way (e.g. by using the int3 instruction on x86_64).
```
