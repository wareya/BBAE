#ifndef BBAE_ABI_H
#define BBAE_ABI_H

#include <stdint.h>

// super primitive ABI / calling convention handling
// - only C compatible for primitives (u8~i64, f32/f64, ptr)
// - not C compatible for structs. structs are always passed by pointer.
// - note that C arrays are identical to pointers to the contained type on function boundaries

// Only ABIs with the following constraints are supported:
// - Only base registers and a single type of floating-point register may be used for register args
// - Spilled arguments go onto the stack monotonically
// - Struct passing logic can only care about:
// -- Whether a struct is packed
// -- Whether each given byte is float-like or not
// - Struct passing logic can only either split the struct up into register-sized chunks or pass it as a pointer
// - Primitive values (i8~i64, f32~f64) are only passed in single registers or on the stack
// - Primitive values are only returned in a single register
// - Struct returns are either returned in a single register or are allocated by the caller and a pointer passed as the first argument, which is returned back to the caller in the same place that pointers are returned
// - "Register size" is the same for integers and floats, i.e. 8 bytes on 64 bit-systems and 4 bytes on 32-bit systems.
// In particular, special logic for passing SIMD vector types is not supported.

// Windows:
// xmm0/rcx    xmm1/rdx    xmm2/r8    xmm3/r9    stack(rtl, top = leftmost)
// structs go into a base register if they're 1, 2, 4, or 8 bytes long. passed as pointers otherwise

// SysV:
// RDI, RSI, RDX, RCX, R8, R9 (nonfloat)
// xmm0~7 (float)
// stack(rtl, top = leftmost)
// horrifyingly, the stack is non-monotonic!!! an arg can go to stack, then the next arg to reg, then the next arg to stack!!!

#define _ABI_RAX 0
#define _ABI_RCX 1
#define _ABI_RDX 2
#define _ABI_RBX 3
#define _ABI_RSP 4
#define _ABI_RBP 5
#define _ABI_RSI 6
#define _ABI_RDI 7
#define _ABI_R8  8
#define _ABI_R9  9
#define _ABI_R10 10
#define _ABI_R11 11
#define _ABI_R12 12
#define _ABI_R13 13
#define _ABI_R14 14
#define _ABI_R15 15

#define _ABI_XMM0  (16 +  0)
#define _ABI_XMM1  (16 +  1)
#define _ABI_XMM2  (16 +  2)
#define _ABI_XMM3  (16 +  3)
#define _ABI_XMM4  (16 +  4)
#define _ABI_XMM5  (16 +  5)
#define _ABI_XMM6  (16 +  6)
#define _ABI_XMM7  (16 +  7)
#define _ABI_XMM8  (16 +  8)
#define _ABI_XMM9  (16 +  9)
#define _ABI_XMM10 (16 + 10)
#define _ABI_XMM11 (16 + 11)
#define _ABI_XMM12 (16 + 12)
#define _ABI_XMM13 (16 + 13)
#define _ABI_XMM14 (16 + 14)
#define _ABI_XMM15 (16 + 15)

enum {
    ABI_WIN,
    ABI_SYSV,
};

#ifdef _WIN32
static uint8_t abi = ABI_WIN;
#else
static uint8_t abi = ABI_SYSV;
#endif

static size_t abi_i64s_used = 0;
static size_t abi_f64s_used = 0;
static size_t abi_stack_used = 0;
static inline void abi_reset_state(void)
{
    abi_i64s_used = 0;
    abi_f64s_used = 0;
    abi_stack_used = 0;
}

// Return value has the following format:
// 
// The bottom 63 bits are used as follows:
// 1) a positive register value from 0 to 31
//    - values from 0 to 15 indicate base registers
//    - values from 16 to 31 indicates floating-point registers
// 2) a negative RBP offset value
//      -40    : RBP+40
//      -16    : RBP+16
// etc.
//
// The top bit indicates that the argument is passed by pointer, not value.
//
// The RBP offsets are the offsets used by the callee after running its 
//      push    rbp
//      mov     rbp, rsp
// prelude. So, they're 16 off from what the stack is at immediately before the `call` instruction begins to execute.
// In other words, the leftmost stack argument is pushed to the stack last before calling.
//
// On windows ABI, RBP offsets start at +48, because:
// - 8 bytes are reserved for the old RBP before copying RSP into RBP
// - 8 bytes are reserved for the return address
// - the next 32 bytes are reserved for the callee to use freely
//
// On sysv ABI, RBP offsets start at +16, because:
// - 8 bytes are reserved for the old RBP before copying RSP into RBP
// - 8 bytes are reserved for the return address
//
// Later stack-spill arguments always have higher offsets, e.g. the first may be +48, second may be +56, etc.
// This is true both on sysv and windows.
//
static inline int64_t abi_get_next_arg_basic(uint8_t word_is_float)
{
    if (abi == ABI_WIN)
    {
        size_t used = abi_i64s_used;
        abi_i64s_used += 1;
        if (used == 0)
            return (!word_is_float) ? _ABI_RCX : _ABI_XMM0;
        else if (used == 1)
            return (!word_is_float) ? _ABI_RDX : _ABI_XMM1;
        else if (used == 2)
            return (!word_is_float) ? _ABI_R8  : _ABI_XMM2;
        else if (used == 3)
            return (!word_is_float) ? _ABI_R9  : _ABI_XMM3;
        else // used >= 4
            return -(48 + (used - 4) * 8);
    }
    else
    {
        if (word_is_float)
        {
            size_t used = abi_f64s_used;
            abi_f64s_used += 1;
            if (used < 8)
                return _ABI_XMM0 + used;
            
            size_t offset = 16 + abi_stack_used;
            abi_stack_used += 8;
            return -offset;
        }
        else
        {
            size_t used = abi_i64s_used;
            abi_i64s_used += 1;
            
            // RDI, RSI, RDX, RCX, R8, R9 (nonfloat)
            if (used == 0)
                return _ABI_RDI;
            else if (used == 1)
                return _ABI_RSI;
            else if (used == 2)
                return _ABI_RDX;
            else if (used == 3)
                return _ABI_RCX;
            else if (used == 4)
                return _ABI_R8;
            else if (used == 5)
                return _ABI_R9;
            
            size_t offset = 16 + abi_stack_used;
            abi_stack_used += 8;
            return -offset;
        }
    }
}

// eightwords_info points to at least count bytes, 0 = not floatlike, 1 = floatlike
// out points to at least count elements
/*
void abi_get_next_args_struct(uint8_t * eightwords_info, uint64_t * out, size_t count)
{
    
}
*/

static inline int64_t abi_get_min_stack_size(void)
{
    if (abi == ABI_WIN)
        return 40;
    else
        return 8;
}

static inline uint64_t abi_get_clobber_mask(void)
{
    uint64_t clobber = 0;
    if (abi == ABI_WIN)
    {
        clobber = (1 << _ABI_RAX ) | (1 << _ABI_RCX ) | (1 << _ABI_RDX ) |
                  (1 << _ABI_R8  ) | (1 << _ABI_R9  ) | (1 << _ABI_R10 ) | (1 << _ABI_R11) |
                  (1 << _ABI_XMM0) | (1 << _ABI_XMM1) | (1 << _ABI_XMM2) |
                  (1 << _ABI_XMM3) | (1 << _ABI_XMM4) | (1 << _ABI_XMM5);
    }
    else
    {
        clobber = (1 << _ABI_RAX  ) | (1 << _ABI_RCX  ) | (1 << _ABI_RDX  ) | (1 << _ABI_RSI  ) | (1 << _ABI_RDI ) |
                  (1 << _ABI_R8   ) | (1 << _ABI_R9   ) | (1 << _ABI_R10  ) | (1 << _ABI_R11  ) |
                  (1 << _ABI_XMM0 ) | (1 << _ABI_XMM1 ) | (1 << _ABI_XMM2 ) | (1 << _ABI_XMM3 ) |
                  (1 << _ABI_XMM4 ) | (1 << _ABI_XMM5 ) | (1 << _ABI_XMM6 ) | (1 << _ABI_XMM7 ) |
                  (1 << _ABI_XMM8 ) | (1 << _ABI_XMM9 ) | (1 << _ABI_XMM10) | (1 << _ABI_XMM11) |
                  (1 << _ABI_XMM12) | (1 << _ABI_XMM13) | (1 << _ABI_XMM14) | (1U << _ABI_XMM15);
    }
    return clobber;
}

// Values with indexes corresponding to callee-saved registers are multiplied by 2.
static inline void abi_get_callee_saved_regs(uint8_t * registers, size_t max_count)
{
    uint64_t clobber = abi_get_clobber_mask();
    for (size_t i = 0; i < 32 && i < max_count; i++)
    {
        uint8_t n = !((clobber >> i) & 1);
        registers[i] *= n + 1;
    }
}

static inline uint8_t abi_reg_is_callee_saved(uint64_t reg)
{
    assert(reg < 32);
    uint64_t clobber = abi_get_clobber_mask();
    return !!((clobber >> reg) & 1);
}

#endif // BBAE_ABI_H
