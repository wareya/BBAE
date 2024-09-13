# BBAE

BBAE (Basic Block Analysis Enabler) is a WIP low-level compiler backend language, both a spec and a basic implementation. It sits between programming languages and the hardware, with a format that's easier to optimize than high-level code. The reference implementation is a C99/C++17 program and/or header set (can be used as either).

## Design philosophy

BBAE is heavily inspired by LLVM, but aims to be much simpler and more intuitive. For example, it uses "block arguments" instead of "phi nodes". It also avoids symbols in its textual syntax, has a simpler type system, allows slightly more types of blocks, and aims to have as few features as is necessary rather than trying to support every single edge case. It's also intended to be relatively easy to reimplement, and specification-defined, rather than being stuck with the canonical implementation forever.

## Current status

BBAE has a reference implementation that can compile and run basic textual programs, which have a format that looks like the following (a partial pi calculation program).

The compiler is largely unfinished, and is missing a lot of important instructions and optimizations.

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

### BBEL

I'm also working on a toy high-level language called BBEL (BBAE Basic Example Language) that cn be studied as a reference for how to interface with BBAE via an LLVM-style "builder" API. It can currently compile and run programs that look like this:

```c
i64 main(i64 a, i64 b)
{
    i64 x = 5;
    x = a + x;
    return x;
}
```

BBEL is implemented in C++. Here's an example of what its code looks like, with BBAE builder API function calls noted:

```c++
// in function:  void compile(CompilerState & state, std::shared_ptr<ASTNode> ast)
// ...
    else if (*ast->text == "return")
    {
        if (ast->children.size() == 0)
        {
            build_return_void(state.current_block); // BBAE builder call
        }
        else if (ast->children.size() == 1)
        {
            compile(state, ast->children[0]);
            auto a = state.stack.back();
            state.stack.pop_back();
            
            assert(a);
            
            build_return_1val(state.current_block, a); // BBAE builder call
        }
        else
            assert(0);
        
        state.current_block = create_block(state.program, 0); // BBAE builder call
    }
// ...
```

## License

(Some files, like those under the `thirdparty` directory, may be licensed under other licenses.)

Copyright "Wareya" (wareya@gmail.com) and any contributors

Licensed under the Apache License v2.0, with LLVM Exceptions and an additional custom exception that makes the license more permissive. The custom exception may be removed, allowing you to use this software under the SPDX-identified Apache-2.0 WITH LLVM-exception license. See LICENSE.txt and the below License Exceptions section for details.

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

## License Exceptions

This section is not legal code, it's a human-readable summary.

This software is licensed with the LLVM exceptions on top of the Apache 2.0 license. These exceptions make it compatible with the GNU GPLv2 and also waive certain restrictions when distributing binaries.

This software is licensed with an additional, custom exception that makes the Apache 2.0 license more permissive by not requiring modified source files to be marked with prominent notices. This exception can be removed, turning the license into pure Apache-2.0 WITH LLVM-exception. In other words, as a user or downstream project or dependent, you can ignore this exception's existence, and as a contributor or maintainer, it means that you have one less responsibility.

These exceptions do not necessarily apply to any dependencies or dependents of this software, unless they independently have the same or similar exceptions.
