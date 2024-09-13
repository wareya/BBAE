# BBAE

BBAE (Basic Block Analysis Enabler) is a low-level compiler backend language, both a spec and a basic implementation. It sits between programming languages and the hardware, with a format that's easier to optimize than high-level code.

## Design philosophy

BBAE is heavily inspired by LLVM, but aims to be much simpler and more intuitive. For example, it uses "block arguments" instead of "phi nodes". It also avoids symbols in its textual syntax, has a simpler type system, allows slightly more types of blocks, and aims to have as few features as is necessary rather than trying to support every single edge case. It's also intended to be relatively easy to reimplement, and specification-defined, rather than being stuck with the canonical implementation forever.

## Current status

BBAE has a reference implementation that can compile and run basic textual programs, which have a format that looks like this (partial pi calculation):

```rs
func main returns f64
    arg arg_n i64
    stack_slot sum 8
    stack_slot flip 8
    stack_slot i 8
    stack_slot n 8
    store sum 0i64
    store flip -1.0f64
    store i 1i64
    store n 1000000000i64
    goto loop_head
block loop_head
    nval = load i64 n
    ival = load i64 i
    
    f = load f64 flip
    f2 = fmul f -1.0f64
    store flip f2
    
    a = shl ival 1i64
    b = sub a 1i64
    c = uint_to_float f64 b
    d = fdiv f2 c
    
    s = load f64 sum
    s2 = fadd s d
    store sum s2
    
    ival2 = add ival 1i64
    
    cmp = cmp_g ival2 nval
    if cmp goto out
    
    store i ival2
    
    goto loop_head
block out
    ret = load f64 sum
    ret2 = fmul ret 4.0f64
    return ret2
endfunc
```

The compiler is largely unfinished, and is missing a lot of important instructions and optimizations.

I'm also working on a toy high-level language called BBEL (BBAE Basic Example Language) that cn be studied as a reference for how to interface with BBAE via an LLVM-style "builder" API. It can currently compile and run programs that look like this:

```c
i64 main(i64 a, i64 b)
{
    i64 x = 5;
    x = a + x;
    return x;
}
```
