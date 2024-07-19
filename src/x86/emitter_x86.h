#ifndef _BBAE_EMITTER_H
#define _BBAE_EMITTER_H

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "../../thirdparty/zydis/Zydis.h"
#include "../../thirdparty/zydis/Zydis.c"

#include "../buffers.h"

typedef ZydisEncoderOperand EncOperand;

static uint8_t encops_equal(EncOperand a, EncOperand b)
{
    return memcmp(&a, &b, sizeof(EncOperand)) == 0;
}

enum Register {
    REG_RAX,
    REG_RCX,
    REG_RDX,
    REG_RBX,
    REG_RSP,
    REG_RBP,
    REG_RSI,
    REG_RDI,
    
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    
    REG_XMM0,
    REG_XMM1,
    REG_XMM2,
    REG_XMM3,
    REG_XMM4,
    REG_XMM5,
    REG_XMM6,
    REG_XMM7,
    
    REG_XMM8,
    REG_XMM9,
    REG_XMM10,
    REG_XMM11,
    REG_XMM12,
    REG_XMM13,
    REG_XMM14,
    REG_XMM15,
    
    REG_RIP,
    
    REG_NONE,
};

/*
enum Segment {
    SEG_ES,
    SEG_CS,
    SEG_SS,
    SEG_DS,
    SEG_FS,
    SEG_GS,
};
*/
enum InstName {
    INST_ADD,
    INST_ADDPD,
    INST_ADDPS,
    INST_ADDSD,
    INST_ADDSS,
    
    INST_ADDSUBPD,
    INST_ADDSUBPS,
    
    INST_AND,
    
    INST_BT,
    INST_BTC,
    INST_BTR,
    INST_BTS,
    
    INST_CMOVO,
    INST_CMOVNO,
    INST_CMOVB,
    INST_CMOVNB,
    INST_CMOVZ,
    INST_CMOVNZ,
    INST_CMOVBE,
    INST_CMOVNBE,
    INST_CMOVS,
    INST_CMOVNS,
    INST_CMOVP,
    INST_CMOVNP,
    INST_CMOVL,
    INST_CMOVNL,
    INST_CMOVLE,
    INST_CMOVNLE,
    
    INST_CMP,
    INST_CMPPD,
    INST_CMPPS,
    INST_CMPSB,
    INST_CMPSD,
    INST_CMPSQ,
    INST_CMPSS,
    INST_CMPSW,
    
    INST_CPUID,
    
    INST_CVTSD2SS,
    INST_CVTSI2SD,
    INST_CVTSI2SS,
    INST_CVTSS2SD,
    INST_CVTTSD2SI,
    INST_CVTTSS2SI,
    
    INST_DEC,
    
    INST_DIV,
    INST_DIVPD,
    INST_DIVPS,
    INST_DIVSD,
    INST_DIVSS,
    
    INST_IDIV,
    INST_IMUL,
    INST_INC,
    INST_INT,
    INST_INT1,
    INST_INT3,
    
    INST_JMP,
    INST_JRCXZ,
    
    INST_JO,
    INST_JNO,
    INST_JB,
    INST_JNB,
    INST_JZ,
    INST_JNZ,
    INST_JBE,
    INST_JNBE,
    INST_JS,
    INST_JNS,
    INST_JP,
    INST_JNP,
    INST_JL,
    INST_JNL,
    INST_JLE,
    INST_JNLE,
    
    INST_LEA,
    INST_LEAVE,
    
    INST_MOV,
    INST_MOVAPD,
    INST_MOVAPS,
    INST_MOVD,
    INST_MOVQ,
    INST_MOVSD,
    INST_MOVUPD,
    INST_MOVUPS,
    
    INST_MUL,
    INST_MULPD,
    INST_MULPS,
    INST_MULSD,
    INST_MULSS,
    
    INST_NEG,
    INST_NOP,
    INST_NOT,
    INST_OR,
    
    INST_POP,
    INST_PUSH,
    INST_RET,
    
    INST_ROUNDPD,
    INST_ROUNDPS,
    INST_ROUNDSD,
    INST_ROUNDSS,
    
    INST_SETO,
    INST_SETNO,
    INST_SETB,
    INST_SETNB,
    INST_SETZ,
    INST_SETNZ,
    INST_SETBE,
    INST_SETNBE,
    INST_SETS,
    INST_SETNS,
    INST_SETP,
    INST_SETNP,
    INST_SETL,
    INST_SETNL,
    INST_SETLE,
    INST_SETNLE,
    
    INST_SHL,
    INST_SHR,
    INST_SAR,
    
    INST_SHUFPD,
    INST_SHUFPS,
    
    INST_SQRTPD,
    INST_SQRTPS,
    INST_SQRTSD,
    INST_SQRTSS,
    
    INST_SUB,
    INST_SUBPD,
    INST_SUBPS,
    INST_SUBSD,
    INST_SUBSS,
    
    INST_TEST,
    
    INST_UCOMISD,
    INST_UCOMISS,
    
    INST_XCHG,
    
    INST_XOR,
    INST_XORPS,
};

static int name_to_mnemonic(int name)
{
    switch (name)
    {
        case INST_ADD       : return ZYDIS_MNEMONIC_ADD;
        case INST_ADDPD     : return ZYDIS_MNEMONIC_ADDPD;
        case INST_ADDPS     : return ZYDIS_MNEMONIC_ADDPS;
        case INST_ADDSD     : return ZYDIS_MNEMONIC_ADDSD;
        case INST_ADDSS     : return ZYDIS_MNEMONIC_ADDSS;
        
        case INST_ADDSUBPD  : return ZYDIS_MNEMONIC_ADDSUBPD;
        case INST_ADDSUBPS  : return ZYDIS_MNEMONIC_ADDSUBPS;
        
        case INST_AND       : return ZYDIS_MNEMONIC_AND;
        
        case INST_BT        : return ZYDIS_MNEMONIC_BT;
        case INST_BTC       : return ZYDIS_MNEMONIC_BTC;
        case INST_BTR       : return ZYDIS_MNEMONIC_BTR;
        case INST_BTS       : return ZYDIS_MNEMONIC_BTS;
        
        case INST_CMOVO     : return ZYDIS_MNEMONIC_CMOVO;
        case INST_CMOVNO    : return ZYDIS_MNEMONIC_CMOVNO;
        case INST_CMOVB     : return ZYDIS_MNEMONIC_CMOVB;
        case INST_CMOVNB    : return ZYDIS_MNEMONIC_CMOVNB;
        case INST_CMOVZ     : return ZYDIS_MNEMONIC_CMOVZ;
        case INST_CMOVNZ    : return ZYDIS_MNEMONIC_CMOVNZ;
        case INST_CMOVBE    : return ZYDIS_MNEMONIC_CMOVBE;
        case INST_CMOVNBE   : return ZYDIS_MNEMONIC_CMOVNBE;
        case INST_CMOVS     : return ZYDIS_MNEMONIC_CMOVS;
        case INST_CMOVNS    : return ZYDIS_MNEMONIC_CMOVNS;
        case INST_CMOVP     : return ZYDIS_MNEMONIC_CMOVP;
        case INST_CMOVNP    : return ZYDIS_MNEMONIC_CMOVNP;
        case INST_CMOVL     : return ZYDIS_MNEMONIC_CMOVL;
        case INST_CMOVNL    : return ZYDIS_MNEMONIC_CMOVNL;
        case INST_CMOVLE    : return ZYDIS_MNEMONIC_CMOVLE;
        case INST_CMOVNLE   : return ZYDIS_MNEMONIC_CMOVNLE;
        
        case INST_CMP       : return ZYDIS_MNEMONIC_CMP;
        case INST_CMPPD     : return ZYDIS_MNEMONIC_CMPPD;
        case INST_CMPPS     : return ZYDIS_MNEMONIC_CMPPS;
        case INST_CMPSB     : return ZYDIS_MNEMONIC_CMPSB;
        case INST_CMPSD     : return ZYDIS_MNEMONIC_CMPSD;
        case INST_CMPSQ     : return ZYDIS_MNEMONIC_CMPSQ;
        case INST_CMPSS     : return ZYDIS_MNEMONIC_CMPSS;
        case INST_CMPSW     : return ZYDIS_MNEMONIC_CMPSW;
        
        case INST_CPUID     : return ZYDIS_MNEMONIC_CPUID;
        
        case INST_CVTSD2SS  : return ZYDIS_MNEMONIC_CVTSD2SS;
        case INST_CVTSI2SD  : return ZYDIS_MNEMONIC_CVTSI2SD;
        case INST_CVTSI2SS  : return ZYDIS_MNEMONIC_CVTSI2SS;
        case INST_CVTSS2SD  : return ZYDIS_MNEMONIC_CVTSS2SD;
        case INST_CVTTSD2SI : return ZYDIS_MNEMONIC_CVTTSD2SI;
        case INST_CVTTSS2SI : return ZYDIS_MNEMONIC_CVTTSS2SI;
        
        case INST_DEC       : return ZYDIS_MNEMONIC_DEC;
        
        case INST_DIV       : return ZYDIS_MNEMONIC_DIV;
        case INST_DIVPD     : return ZYDIS_MNEMONIC_DIVPD;
        case INST_DIVPS     : return ZYDIS_MNEMONIC_DIVPS;
        case INST_DIVSD     : return ZYDIS_MNEMONIC_DIVSD;
        case INST_DIVSS     : return ZYDIS_MNEMONIC_DIVSS;
        
        case INST_IDIV      : return ZYDIS_MNEMONIC_IDIV;
        case INST_IMUL      : return ZYDIS_MNEMONIC_IMUL;
        case INST_INC       : return ZYDIS_MNEMONIC_INC;
        case INST_INT       : return ZYDIS_MNEMONIC_INT;
        case INST_INT1      : return ZYDIS_MNEMONIC_INT1;
        case INST_INT3      : return ZYDIS_MNEMONIC_INT3;
        
        case INST_JMP       : return ZYDIS_MNEMONIC_JMP;
        case INST_JRCXZ     : return ZYDIS_MNEMONIC_JRCXZ;
        
        case INST_JO        : return ZYDIS_MNEMONIC_JO;
        case INST_JNO       : return ZYDIS_MNEMONIC_JNO;
        case INST_JB        : return ZYDIS_MNEMONIC_JB;
        case INST_JNB       : return ZYDIS_MNEMONIC_JNB;
        case INST_JZ        : return ZYDIS_MNEMONIC_JZ;
        case INST_JNZ       : return ZYDIS_MNEMONIC_JNZ;
        case INST_JBE       : return ZYDIS_MNEMONIC_JBE;
        case INST_JNBE      : return ZYDIS_MNEMONIC_JNBE;
        case INST_JS        : return ZYDIS_MNEMONIC_JS;
        case INST_JNS       : return ZYDIS_MNEMONIC_JNS;
        case INST_JP        : return ZYDIS_MNEMONIC_JP;
        case INST_JNP       : return ZYDIS_MNEMONIC_JNP;
        case INST_JL        : return ZYDIS_MNEMONIC_JL;
        case INST_JNL       : return ZYDIS_MNEMONIC_JNL;
        case INST_JLE       : return ZYDIS_MNEMONIC_JLE;
        case INST_JNLE      : return ZYDIS_MNEMONIC_JNLE;
        
        case INST_LEA       : return ZYDIS_MNEMONIC_LEA;
        case INST_LEAVE     : return ZYDIS_MNEMONIC_LEAVE;
        
        case INST_MOV       : return ZYDIS_MNEMONIC_MOV;
        case INST_MOVAPD    : return ZYDIS_MNEMONIC_MOVAPD;
        case INST_MOVAPS    : return ZYDIS_MNEMONIC_MOVAPS;
        case INST_MOVD      : return ZYDIS_MNEMONIC_MOVD;
        case INST_MOVQ      : return ZYDIS_MNEMONIC_MOVQ;
        case INST_MOVSD     : return ZYDIS_MNEMONIC_MOVSD;
        case INST_MOVUPD    : return ZYDIS_MNEMONIC_MOVUPD;
        case INST_MOVUPS    : return ZYDIS_MNEMONIC_MOVUPS;
        
        case INST_MUL       : return ZYDIS_MNEMONIC_MUL;
        case INST_MULPD     : return ZYDIS_MNEMONIC_MULPD;
        case INST_MULPS     : return ZYDIS_MNEMONIC_MULPS;
        case INST_MULSD     : return ZYDIS_MNEMONIC_MULSD;
        case INST_MULSS     : return ZYDIS_MNEMONIC_MULSS;
        
        case INST_NEG       : return ZYDIS_MNEMONIC_NEG;
        case INST_NOP       : return ZYDIS_MNEMONIC_NOP;
        case INST_NOT       : return ZYDIS_MNEMONIC_NOT;
        case INST_OR        : return ZYDIS_MNEMONIC_OR;
        
        case INST_POP       : return ZYDIS_MNEMONIC_POP;
        case INST_PUSH      : return ZYDIS_MNEMONIC_PUSH;
        case INST_RET       : return ZYDIS_MNEMONIC_RET;
        
        case INST_ROUNDPD   : return ZYDIS_MNEMONIC_ROUNDPD;
        case INST_ROUNDPS   : return ZYDIS_MNEMONIC_ROUNDPS;
        case INST_ROUNDSD   : return ZYDIS_MNEMONIC_ROUNDSD;
        case INST_ROUNDSS   : return ZYDIS_MNEMONIC_ROUNDSS;
        
        case INST_SETO      : return ZYDIS_MNEMONIC_SETO;
        case INST_SETNO     : return ZYDIS_MNEMONIC_SETNO;
        case INST_SETB      : return ZYDIS_MNEMONIC_SETB;
        case INST_SETNB     : return ZYDIS_MNEMONIC_SETNB;
        case INST_SETZ      : return ZYDIS_MNEMONIC_SETZ;
        case INST_SETNZ     : return ZYDIS_MNEMONIC_SETNZ;
        case INST_SETBE     : return ZYDIS_MNEMONIC_SETBE;
        case INST_SETNBE    : return ZYDIS_MNEMONIC_SETNBE;
        case INST_SETS      : return ZYDIS_MNEMONIC_SETS;
        case INST_SETNS     : return ZYDIS_MNEMONIC_SETNS;
        case INST_SETP      : return ZYDIS_MNEMONIC_SETP;
        case INST_SETNP     : return ZYDIS_MNEMONIC_SETNP;
        case INST_SETL      : return ZYDIS_MNEMONIC_SETL;
        case INST_SETNL     : return ZYDIS_MNEMONIC_SETNL;
        case INST_SETLE     : return ZYDIS_MNEMONIC_SETLE;
        case INST_SETNLE    : return ZYDIS_MNEMONIC_SETNLE;
        
        case INST_SHL       : return ZYDIS_MNEMONIC_SHL;
        case INST_SHR       : return ZYDIS_MNEMONIC_SHR;
        case INST_SAR       : return ZYDIS_MNEMONIC_SAR;
        
        case INST_SHUFPD    : return ZYDIS_MNEMONIC_SHUFPD;
        case INST_SHUFPS    : return ZYDIS_MNEMONIC_SHUFPS;
        
        case INST_SQRTPD    : return ZYDIS_MNEMONIC_SQRTPD;
        case INST_SQRTPS    : return ZYDIS_MNEMONIC_SQRTPS;
        case INST_SQRTSD    : return ZYDIS_MNEMONIC_SQRTSD;
        case INST_SQRTSS    : return ZYDIS_MNEMONIC_SQRTSS;
        
        case INST_SUB       : return ZYDIS_MNEMONIC_SUB;
        case INST_SUBPD     : return ZYDIS_MNEMONIC_SUBPD;
        case INST_SUBPS     : return ZYDIS_MNEMONIC_SUBPS;
        case INST_SUBSD     : return ZYDIS_MNEMONIC_SUBSD;
        case INST_SUBSS     : return ZYDIS_MNEMONIC_SUBSS;
        
        case INST_TEST      : return ZYDIS_MNEMONIC_TEST;
        
        case INST_UCOMISD   : return ZYDIS_MNEMONIC_UCOMISD;
        case INST_UCOMISS   : return ZYDIS_MNEMONIC_UCOMISS;
        
        case INST_XCHG      : return ZYDIS_MNEMONIC_XCHG;
        
        case INST_XOR       : return ZYDIS_MNEMONIC_XOR;
        case INST_XORPS     : return ZYDIS_MNEMONIC_XORPS;
        default: assert(((void)"unknown instruction!", 0));
    }
}

static int reg_unsized_to_sized(enum Register reg, int size)
{
    if (reg == REG_NONE)
        return ZYDIS_REGISTER_NONE;
    
    assert(size == 1 || size == 2 || size == 4 || size == 8);
    if (reg >= REG_XMM0)
        assert(size == 8);
    
    if (reg == REG_RIP)
        return ZYDIS_REGISTER_RIP;
    
    if (reg >= REG_XMM0)
        return ZYDIS_REGISTER_XMM0 + ((int)reg - REG_XMM0);
    else if (size == 8)
        return ZYDIS_REGISTER_RAX + (int)reg;
    else if (size == 4)
        return ZYDIS_REGISTER_EAX + (int)reg;
    else if (size == 2)
        return ZYDIS_REGISTER_AX + (int)reg;
    else if (size == 1 && reg <= REG_RBX)
        return ZYDIS_REGISTER_AL + (int)reg;
    else if (size == 1 && reg > REG_RBX)
        return ZYDIS_REGISTER_AL + (int)reg + 4;
    else
        assert(((void)"unknown register!\n", 0));
}

static EncOperand zy_reg(enum Register reg, int size)
{
    EncOperand ret;
    memset(&ret, 0, sizeof(ret)); 
    
    ret.type = ZYDIS_OPERAND_TYPE_REGISTER;
    
    ret.reg.value = reg_unsized_to_sized(reg, size);
    
    return ret;
}
static EncOperand zy_mem_full(enum Register base_reg, enum Register index_reg, int index_scale, int64_t offset, int word_size)
{
    assert(index_scale == 0 || index_scale == 1 || index_scale == 2 || index_scale == 4 || index_scale == 8);
    
    EncOperand ret;
    memset(&ret, 0, sizeof(ret)); 
    
    ret.type = ZYDIS_OPERAND_TYPE_MEMORY;
    
    ret.mem.base = reg_unsized_to_sized(base_reg, 8);
    ret.mem.index = reg_unsized_to_sized(index_reg, 8);
    ret.mem.scale = index_scale;
    ret.mem.displacement = offset;
    ret.mem.size = word_size;
    
    return ret;
}
static EncOperand zy_mem(enum Register base_reg, int64_t offset, int word_size)
{
    return zy_mem_full(base_reg, REG_NONE, 0, offset, word_size);
}

static EncOperand zy_mem_add_offset(EncOperand op, uint64_t offset)
{
    assert(op.type == ZYDIS_OPERAND_TYPE_MEMORY);
    op.mem.displacement += offset;
    return op;
}

static EncOperand zy_mem_change_size(EncOperand op, uint64_t size)
{
    assert(op.type == ZYDIS_OPERAND_TYPE_MEMORY);
    assert(size == 1 || size == 2 || size == 4 || size == 8);
    op.mem.size = size;
    return op;
}
/*
EncOperand zy_ptr(enum Segment segment, int64_t offset, int word_size)
{
    assert(index_scale == 1 || index_scale == 2 || index_scale == 4 || index_scale == 8);
    
    EncOperand ret;
    memset(&ret, 0, sizeof(ret)); 
    
    ret.mem.segment = segment;
    ret.mem.offset = offset;
    
    return ret;
}
*/
static EncOperand zy_imm(uint64_t imm, uint64_t size)
{
    assert(size == 1 || size == 2 || size == 4 || size == 8);
    EncOperand ret;
    memset(&ret, 0, sizeof(ret));
    
    if (size == 1)
    {
        int8_t n = imm;
        imm = (int64_t)n;
    }
    else if (size == 2)
    {
        int16_t n = imm;
        imm = (int16_t)n;
    }
    else if (size == 4)
    {
        int32_t n = imm;
        imm = (int32_t)n;
    }
    
    ret.type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
    ret.imm.u = imm;
    return ret;
}


const char * ZyanStatusText(ZyanStatus status)
{
    switch (status)
    {
        case 0x00: return "NO_MORE_DATA";
        case 0x01: return "DECODING_ERROR";
        case 0x02: return "INSTRUCTION_TOO_LONG";
        case 0x03: return "BAD_REGISTER";
        case 0x04: return "ILLEGAL_LOCK";
        case 0x05: return "ILLEGAL_LEGACY_PFX";
        case 0x06: return "ILLEGAL_REX";
        case 0x07: return "INVALID_MAP";
        case 0x08: return "MALFORMED_EVEX";
        case 0x09: return "MALFORMED_MVEX";
        case 0x0A: return "INVALID_MASK";
        case 0x0B: return "SKIP_TOKEN";
        case 0x0C: return "IMPOSSIBLE_INSTRUCTION";
        default: assert(0);
    };
}

static void do_encode(ZydisEncoderRequest req, uint8_t * buf, size_t * len)
{
    req.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
    req.allowed_encodings =
        ZYDIS_ENCODABLE_ENCODING_LEGACY |
        ZYDIS_ENCODABLE_ENCODING_3DNOW |
        ZYDIS_ENCODABLE_ENCODING_XOP |
        ZYDIS_ENCODABLE_ENCODING_VEX;
    
    *len = ZYDIS_MAX_INSTRUCTION_LENGTH;
    
    ZyanStatus e = ZydisEncoderEncodeInstruction(&req, buf, len);
    if (ZYAN_FAILED(e))
    {
        printf("culprit: %s\n", ZyanStatusText(e));
        assert(((void)"Failed to encode instruction", 0));
    }
}

static void zy_emit_n(byte_buffer * bytes, int name, EncOperand * ops, int n)
{
    assert(n <= 5);
    
    ZydisEncoderRequest req;
    memset(&req, 0, sizeof(req));
    
    req.mnemonic = name_to_mnemonic(name);
    req.operand_count = n;
    for (int i = 0; i < n; i++)
        req.operands[i] = ops[i];
    
    uint8_t buf[ZYDIS_MAX_INSTRUCTION_LENGTH];
    size_t len = 0;
    do_encode(req, buf, &len);
    bytes_push(bytes, buf, len);
}

static void zy_emit_4(byte_buffer * bytes, int name, EncOperand op1, EncOperand op2, EncOperand op3, EncOperand op4)
{
    EncOperand ops[] = {op1, op2, op3, op4};
    zy_emit_n(bytes, name, ops, 4);
}
static void zy_emit_3(byte_buffer * bytes, int name, EncOperand op1, EncOperand op2, EncOperand op3)
{
    EncOperand ops[] = {op1, op2, op3};
    zy_emit_n(bytes, name, ops, 3);
}
static void zy_emit_2(byte_buffer * bytes, int name, EncOperand op1, EncOperand op2)
{
    if (name == INST_MOV && memcmp(&op1, &op2, sizeof(EncOperand)) == 0)
        return;
    EncOperand ops[] = {op1, op2};
    zy_emit_n(bytes, name, ops, 2);
}
static void zy_emit_1(byte_buffer * bytes, int name, EncOperand op1)
{
    EncOperand ops[] = {op1};
    zy_emit_n(bytes, name, ops, 1);
}
static void zy_emit_0(byte_buffer * bytes, int name)
{
    zy_emit_n(bytes, name, 0, 0);
}

#endif // _BBAE_EMITTER_H
