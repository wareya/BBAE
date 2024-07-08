



// RAX    X    YES
// RCX    X    YES
// RDX    X    YES
// RBX    X    YES
// RSP    .    no
// RBP    .    no
// RSI    X    YES
// RDI    X    YES
// R8     X    YES
// R9     X    YES
// R10    X    YES
// R11    X    YES
// R12    X    YES
// R13    X    YES
// R14    X    YES
// R15    X    YES

uint64_t allocable_int[] = {
    0, 1, 2, 3,
    6, 7,
    8, 9, 10, 11, 12, 13, 14, 15
};
uint64_t allocable_float[] = {
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31
};

// XMM0  YES (preferred)
// XMM1  YES (preferred)
// XMM2  YES (preferred)
// XMM3  YES (preferred)
// XMM4  YES (preferred)
// XMM5  YES (preferred)
// XMM6  YES
// XMM7  YES
// XMM8  YES
// XMM9  YES
// XMM10 YES
// XMM11 YES
// XMM12 YES
// XMM13 YES
// XMM14 YES
// XMM15 YES