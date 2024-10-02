#ifndef _BBAE_EMITTER_H
#define _BBAE_EMITTER_H

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "../buffers.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define enc_imm fadec_enc_imm

#ifdef __cplusplus
#define restrict __restrict
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wc++20-extensions"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wc99-designator"
#endif
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../../thirdparty/fadec/fadec-enc.h"
#include "../../thirdparty/fadec/encode.c"
#pragma GCC diagnostic pop

#ifdef __cplusplus
#undef restrict
#endif

#undef enc_imm

#define FE_ISMEM(x) (!!((x).op & INT64_MIN))
#define FE_ISREG(x) (!FE_ISMEM(x))
#define FE_ISREG(x) (!FE_ISMEM(x))
#define FE_ISGEN(x) ((x).op >= FE_AX && (x).op <= FE_R15)
#define FE_ISSSE(x) ((x).op >= FE_XMM0 && (x).op <= FE_XMM15)

typedef struct _EncOperand {
    uint64_t op;
    uint8_t size;
    uint8_t is_imm;
} EncOperand;

static inline uint8_t encops_equal(EncOperand a, EncOperand b)
{
    return memcmp(&a, &b, sizeof(int64_t) + 1) == 0;
}

#ifdef __cplusplus
}
#endif

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
    
    INST_CALL,
    
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
    //INST_CMPPD,
    //INST_CMPPS,
    //INST_CMPSD,
    //INST_CMPSS,
    
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
    INST_MOVD, // doesn't support XMM<->XMM moves
    INST_MOVQ, // supports all semantically valid moves
    INST_MOVSD, // doesn't support XMM<->BASEREG non-memory moves
    INST_MOVSS, // doesn't support XMM<->BASEREG non-memory moves
    INST_MOVUPD, // equivalent to MOVUPS, but has more instruction bytes. disprefer.
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
    
    INST_ROL,
    INST_ROR,
    
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
    
    INST_LFENCE,
    INST_MFENCE,
    INST_SFENCE,
};

static inline uint64_t name_to_inst(int name, EncOperand * ops, int n)
{
    #define _BBAE_SSELIKE_BIT(NAME) FE_ISMEM(ops[1]) ? NAME##rm : NAME##rr
    
    #define _BBAE_SSEx2(NAME, NAME2) \
        case INST_##NAME##NAME2##D : assert(n == 2); return _BBAE_SSELIKE_BIT(FE_SSE_##NAME##NAME2##D); \
        case INST_##NAME##NAME2##S : assert(n == 2); return _BBAE_SSELIKE_BIT(FE_SSE_##NAME##NAME2##S);
    
    #define _BBAE_SSEFLAGLIKE_BIT(NAME) FE_ISMEM(ops[1]) ? NAME##rmi : NAME##rri
    
    #define _BBAE_SSEFLAGx2(NAME, NAME2) \
        case INST_##NAME##NAME2##D : assert(n == 3); return _BBAE_SSEFLAGLIKE_BIT(FE_SSE_##NAME##NAME2##D); \
        case INST_##NAME##NAME2##S : assert(n == 3); return _BBAE_SSEFLAGLIKE_BIT(FE_SSE_##NAME##NAME2##S);
    
    #define _BBAE_SSEFULLLIKE_BIT(NAME) FE_ISMEM(ops[1]) ? NAME##rm : FE_ISMEM(ops[0]) ? NAME##mr : NAME##rr
    
    #define _BBAE_SSEFULL_x2(NAME, NAME2) \
        case INST_##NAME##NAME2##D : assert(n == 2); return _BBAE_SSEFULLLIKE_BIT(FE_SSE_##NAME##NAME2##D); \
        case INST_##NAME##NAME2##S : assert(n == 2); return _BBAE_SSEFULLLIKE_BIT(FE_SSE_##NAME##NAME2##S);
    
    #define _BBAE_ADDLIKE_BIT(NAME) ( \
        FE_ISMEM(ops[1]) ? NAME##rm : \
        FE_ISMEM(ops[0]) ? ( ops[1].is_imm ? NAME##mi : NAME##mr ) : \
        ops[1].is_imm ? NAME##ri : NAME##rr );
    
    #define _BBAE_ADDLIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm); \
        assert(FE_ISREG(ops[0]) || FE_ISREG(ops[1])); \
        if (ops[0].size == 1) return _BBAE_ADDLIKE_BIT(FE_##NAME##8); \
        if (ops[0].size == 2) return _BBAE_ADDLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_ADDLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_ADDLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_BTLIKE_BIT(NAME) ( \
        FE_ISMEM(ops[0]) ? ( ops[1].is_imm ? NAME##mi : NAME##mr ) : \
        ops[1].is_imm ? NAME##ri : NAME##rr );
    
    #define _BBAE_BTLIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm); \
        assert(FE_ISREG(ops[1])); \
        if (ops[0].size == 2) return _BBAE_BTLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_BTLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_BTLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_DECLIKE_BIT(NAME) FE_ISMEM(ops[0]) ? NAME##m : NAME##r
    
    #define _BBAE_DECLIKE(NAME) { \
        assert(n == 1); \
        assert(!ops[0].is_imm); \
        if (ops[0].size == 1) return _BBAE_DECLIKE_BIT(FE_##NAME##8); \
        if (ops[0].size == 2) return _BBAE_DECLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_DECLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_DECLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_SETLIKE(NAME) { \
        assert(n == 1); \
        assert(ops[0].size == 1); \
        assert(!ops[0].is_imm); \
        return _BBAE_DECLIKE_BIT(FE_##NAME##8); \
    }
    
    #define _BBAE_CMOVLIKE_BIT(NAME) FE_ISMEM(ops[1]) ? NAME##rm : NAME##rr
    
    #define _BBAE_CMOVLIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm && !ops[1].is_imm); \
        assert(FE_ISREG(ops[0])); \
        if (ops[0].size == 2) return _BBAE_CMOVLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_CMOVLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_CMOVLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_CVTLIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm && !ops[1].is_imm); \
        assert(FE_ISREG(ops[0])); \
        if (ops[0].size == 4) return _BBAE_SSELIKE_BIT(FE_SSE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_SSELIKE_BIT(FE_SSE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_LEALIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm); \
        assert(FE_ISREG(ops[0])); \
        assert(FE_ISMEM(ops[1])); \
        if (ops[0].size == 2) return FE_##NAME##16##rm; \
        if (ops[0].size == 4) return FE_##NAME##32##rm; \
        if (ops[0].size == 8) return FE_##NAME##64##rm; \
        assert(0); \
    }
    
    #define _BBAE_SHRLIKE(NAME) { \
        assert(n == 2); \
        assert(ops[1].is_imm || (ops[1].op == FE_CX && ops[1].size == 1)); \
        assert(!ops[0].is_imm); \
        if (ops[0].size == 1) return _BBAE_BTLIKE_BIT(FE_##NAME##8); \
        if (ops[0].size == 2) return _BBAE_BTLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_BTLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_BTLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_TESTLIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm); \
        assert(ops[0].size == ops[1].size); \
        if (ops[0].size == 1) return _BBAE_BTLIKE_BIT(FE_##NAME##8); \
        if (ops[0].size == 2) return _BBAE_BTLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_BTLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_BTLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    #define _BBAE_XCHGLIKE_BIT(NAME) FE_ISMEM(ops[0]) ? NAME##mr : NAME##rr
    
    #define _BBAE_XCHGLIKE(NAME) { \
        assert(n == 2); \
        assert(!ops[0].is_imm && ops[1].is_imm); \
        assert(FE_ISREG(ops[0])); \
        if (ops[0].size == 1) return _BBAE_XCHGLIKE_BIT(FE_##NAME##8); \
        if (ops[0].size == 2) return _BBAE_XCHGLIKE_BIT(FE_##NAME##16); \
        if (ops[0].size == 4) return _BBAE_XCHGLIKE_BIT(FE_##NAME##32); \
        if (ops[0].size == 8) return _BBAE_XCHGLIKE_BIT(FE_##NAME##64); \
        assert(0); \
    }
    
    switch (name)
    {
        case INST_ADD       : _BBAE_ADDLIKE(ADD)
        _BBAE_SSEx2(ADD, P)
        _BBAE_SSEx2(ADD, S)
        
        case INST_ADDSUBPD  : return _BBAE_SSELIKE_BIT(FE_SSE_ADDSUBPD);
        case INST_ADDSUBPS  : return _BBAE_SSELIKE_BIT(FE_SSE_ADDSUBPS);
        
        case INST_AND       : _BBAE_ADDLIKE(AND)
        
        case INST_BT        : _BBAE_BTLIKE(BT)
        case INST_BTC       : _BBAE_BTLIKE(BTC)
        case INST_BTR       : _BBAE_BTLIKE(BTR)
        case INST_BTS       : _BBAE_BTLIKE(BTS)
        
        case INST_CALL      : return FE_CALL;
        
        case INST_CMOVO     : _BBAE_CMOVLIKE(CMOVO)
        case INST_CMOVNO    : _BBAE_CMOVLIKE(CMOVNO)
        case INST_CMOVB     : _BBAE_CMOVLIKE(CMOVC)
        case INST_CMOVNB    : _BBAE_CMOVLIKE(CMOVNC)
        case INST_CMOVZ     : _BBAE_CMOVLIKE(CMOVZ)
        case INST_CMOVNZ    : _BBAE_CMOVLIKE(CMOVNZ)
        case INST_CMOVBE    : _BBAE_CMOVLIKE(CMOVBE)
        case INST_CMOVNBE   : _BBAE_CMOVLIKE(CMOVA)
        case INST_CMOVS     : _BBAE_CMOVLIKE(CMOVS)
        case INST_CMOVNS    : _BBAE_CMOVLIKE(CMOVNS)
        case INST_CMOVP     : _BBAE_CMOVLIKE(CMOVP)
        case INST_CMOVNP    : _BBAE_CMOVLIKE(CMOVNP)
        case INST_CMOVL     : _BBAE_CMOVLIKE(CMOVL)
        case INST_CMOVNL    : _BBAE_CMOVLIKE(CMOVGE)
        case INST_CMOVLE    : _BBAE_CMOVLIKE(CMOVLE)
        case INST_CMOVNLE   : _BBAE_CMOVLIKE(CMOVG)
        
        case INST_CMP       : _BBAE_ADDLIKE(CMP)
        //case INST_CMPPD     : return FE_CMPPD;
        //case INST_CMPPS     : return FE_CMPPS;
        //case INST_CMPSD     : return FE_CMPSD;
        //case INST_CMPSS     : return FE_CMPSS;
        
        case INST_CPUID     : return FE_CPUID;
        
        case INST_CVTSD2SS  : return _BBAE_SSELIKE_BIT(FE_SSE_CVTSD2SS);
        case INST_CVTSS2SD  : return _BBAE_SSELIKE_BIT(FE_SSE_CVTSS2SD);
        case INST_CVTSI2SD  : _BBAE_CVTLIKE(CVTSD2SI)
        case INST_CVTSI2SS  : _BBAE_CVTLIKE(CVTSS2SI)
        case INST_CVTTSD2SI : _BBAE_CVTLIKE(CVTTSD2SI)
        case INST_CVTTSS2SI : _BBAE_CVTLIKE(CVTTSS2SI)
        
        case INST_DEC       : _BBAE_DECLIKE(DEC)
        case INST_DIV       : _BBAE_DECLIKE(DIV)
        
        _BBAE_SSEx2(DIV, P)
        _BBAE_SSEx2(DIV, S)
        
        case INST_IDIV      : _BBAE_DECLIKE(IDIV)
        case INST_IMUL      : _BBAE_DECLIKE(IMUL)
        case INST_INC       : _BBAE_DECLIKE(INC)
        case INST_INT       : assert(ops[0].is_imm); return FE_INTi;
        case INST_INT1      : return FE_INT1;
        case INST_INT3      : return FE_INT3;
        
        case INST_JMP       : return FE_JMP;
        
        case INST_JRCXZ     : return FE_JCXZ;
        
        case INST_JO        : return FE_JO;
        case INST_JNO       : return FE_JNO;
        case INST_JB        : return FE_JC;
        case INST_JNB       : return FE_JNC;
        case INST_JZ        : return FE_JZ;
        case INST_JNZ       : return FE_JNZ;
        case INST_JBE       : return FE_JBE;
        case INST_JNBE      : return FE_JA;
        case INST_JS        : return FE_JS;
        case INST_JNS       : return FE_JNS;
        case INST_JP        : return FE_JP;
        case INST_JNP       : return FE_JNP;
        case INST_JL        : return FE_JL;
        case INST_JNL       : return FE_JGE;
        case INST_JLE       : return FE_JLE;
        case INST_JNLE      : return FE_JG;
        
        case INST_LEA       : _BBAE_LEALIKE(LEA)
        case INST_LEAVE     : return FE_LEAVE;
        
        case INST_MOV       : _BBAE_ADDLIKE(MOV)
        
        _BBAE_SSEFULL_x2(MOV, AP)
        _BBAE_SSEFULL_x2(MOV, UP)
        _BBAE_SSEFULL_x2(MOV, S)
        
        case INST_MOVD      :
        {
            assert(n == 2);
            assert(!ops[0].is_imm && !ops[1].is_imm);
            
            // FIXME
//            assert(FE_ISMEM(ops[0]) || FE_ISREG(ops[1]));
            assert(!(FE_ISMEM(ops[0]) && FE_ISMEM(ops[1])));
            
            assert(FE_ISGEN(ops[0]) != FE_ISGEN(ops[1]));
            if (FE_ISMEM(ops[0])) return FE_SSE_MOVD_X2Gmr;
            if (FE_ISMEM(ops[1])) return FE_SSE_MOVD_G2Xrm;
            if (FE_ISGEN(ops[0])) return FE_SSE_MOVD_X2Grr;
            if (FE_ISGEN(ops[1])) return FE_SSE_MOVD_G2Xrr;
            assert(0);
        }
        case INST_MOVQ      :
        {
            assert(n == 2);
            assert(!ops[0].is_imm && !ops[1].is_imm);
            assert(!(FE_ISMEM(ops[0]) && FE_ISMEM(ops[1])));
            assert(FE_ISSSE(ops[0]) || FE_ISSSE(ops[1]));
            // FIXME
            //printf("%d %d %d %d\n", FE_ISGEN(ops[0]), FE_ISGEN(ops[1]), FE_ISSSE(ops[0]), FE_ISSSE(ops[1]));
            //assert(FE_ISGEN(ops[0]) != FE_ISGEN(ops[1]) || (FE_ISSSE(ops[0]) && FE_ISSSE(ops[1])));
            if (FE_ISSSE(ops[0]) && FE_ISSSE(ops[1]))
                return FE_SSE_MOVQrr;
            if (FE_ISMEM(ops[0])) return FE_SSE_MOVQ_X2Gmr;
            if (FE_ISMEM(ops[1])) return FE_SSE_MOVQ_G2Xrm;
            if (FE_ISGEN(ops[0])) return FE_SSE_MOVQ_X2Grr;
            if (FE_ISGEN(ops[1])) return FE_SSE_MOVQ_G2Xrr;
            assert(0);
        }
        
        case INST_MUL       : _BBAE_DECLIKE(MUL)
        _BBAE_SSEx2(MUL, P)
        _BBAE_SSEx2(MUL, S)
        
        case INST_NEG       : _BBAE_DECLIKE(NEG)
        case INST_NOP       : return FE_NOP;
        case INST_NOT       : _BBAE_DECLIKE(NOT)
        case INST_OR        : _BBAE_ADDLIKE(OR)
        
        case INST_POP       : assert(n == 1); assert(!ops[0].is_imm); return FE_ISREG(ops[0]) ? FE_POPr : FE_POPm;
        case INST_PUSH      : assert(n == 1); return ops[0].is_imm ? FE_PUSHi : FE_ISREG(ops[0]) ? FE_PUSHr : FE_PUSHm;
        case INST_RET       : return FE_RET;
        
        case INST_ROL       : _BBAE_SHRLIKE(ROL)
        case INST_ROR       : _BBAE_SHRLIKE(ROR)
            
        _BBAE_SSEFLAGx2(ROUND, P)
        _BBAE_SSEFLAGx2(ROUND, S)
        
        case INST_SETO      : _BBAE_SETLIKE(SETO)
        case INST_SETNO     : _BBAE_SETLIKE(SETNO)
        case INST_SETB      : _BBAE_SETLIKE(SETC)
        case INST_SETNB     : _BBAE_SETLIKE(SETNC)
        case INST_SETZ      : _BBAE_SETLIKE(SETZ)
        case INST_SETNZ     : _BBAE_SETLIKE(SETNZ)
        case INST_SETBE     : _BBAE_SETLIKE(SETBE)
        case INST_SETNBE    : _BBAE_SETLIKE(SETA)
        case INST_SETS      : _BBAE_SETLIKE(SETS)
        case INST_SETNS     : _BBAE_SETLIKE(SETNS)
        case INST_SETP      : _BBAE_SETLIKE(SETP)
        case INST_SETNP     : _BBAE_SETLIKE(SETNP)
        case INST_SETL      : _BBAE_SETLIKE(SETL)
        case INST_SETNL     : _BBAE_SETLIKE(SETGE)
        case INST_SETLE     : _BBAE_SETLIKE(SETLE)
        case INST_SETNLE    : _BBAE_SETLIKE(SETG)
        
        case INST_SHL       : _BBAE_SHRLIKE(SHL)
        case INST_SHR       : _BBAE_SHRLIKE(SHR)
        case INST_SAR       : _BBAE_SHRLIKE(SAR)
        
        _BBAE_SSEFLAGx2(SHUF, P)
        
        _BBAE_SSEx2(SQRT, P)
        _BBAE_SSEx2(SQRT, S)
        
        case INST_SUB       : _BBAE_ADDLIKE(SUB)
        _BBAE_SSEx2(SUB, P)
        _BBAE_SSEx2(SUB, S)
        
        case INST_TEST      : _BBAE_TESTLIKE(TEST)
        
        _BBAE_SSEx2(UCOMI, S)
        
        case INST_XCHG      : _BBAE_XCHGLIKE(XCHG)
        
        case INST_XOR        : _BBAE_ADDLIKE(XOR)
        case INST_XORPS     : return _BBAE_CMOVLIKE_BIT(FE_SSE_XORPS);
            
        case INST_LFENCE    : return FE_LFENCE;
        case INST_MFENCE    : return FE_MFENCE;
        case INST_SFENCE    : return FE_SFENCE;
        
        default: assert(((void)"unknown instruction!", 0));
    }
    
    #undef _BBAE_SSELIKE_BIT
    #undef _BBAE_SSEx2
    #undef _BBAE_SSEFLAGLIKE_BIT
    #undef _BBAE_SSEFLAGx2
    #undef _BBAE_SSEFULLLIKE_BIT
    #undef _BBAE_SSEFULL_x2
    #undef _BBAE_ADDLIKE_BIT
    #undef _BBAE_ADDLIKE
    #undef _BBAE_BTLIKE_BIT
    #undef _BBAE_BTLIKE
    #undef _BBAE_DECLIKE_BIT
    #undef _BBAE_DECLIKE
    #undef _BBAE_SETLIKE
    #undef _BBAE_CMOVLIKE_BIT
    #undef _BBAE_CMOVLIKE
    #undef _BBAE_CVTLIKE
    #undef _BBAE_LEALIKE
    #undef _BBAE_SHRLIKE
    #undef _BBAE_TESTLIKE
    #undef _BBAE_XCHGLIKE_BIT
    #undef _BBAE_XCHGLIKE
}

static inline int reg_to_reg(enum Register reg, int size)
{
    if (reg == REG_NONE)
        return 0;
    
    assert(size == 1 || size == 2 || size == 4 || size == 8);
    if (reg >= REG_XMM0)
        assert(size == 8);
    
    if (reg == REG_RIP)
        return FE_IP;
    
    if (reg >= REG_XMM0)
        return FE_XMM0 + ((int)reg - REG_XMM0);
    return FE_AX + (int)reg;
}

static inline EncOperand enc_reg(enum Register reg, int size)
{
    EncOperand ret;
    memset(&ret, 0, sizeof(ret)); 
    
    ret.op = reg_to_reg(reg, size);
    ret.size = size;
    ret.is_imm = 0;
    
    return ret;
}
#ifdef __cplusplus
static inline EncOperand enc_reg(uint64_t reg, int size)
{
    return enc_reg((enum Register)reg, size);
}
#endif

static inline EncOperand enc_mem_full(enum Register base_reg, enum Register index_reg, int index_scale, int64_t offset, int word_size)
{
    assert(index_scale == 0 || index_scale == 1 || index_scale == 2 || index_scale == 4 || index_scale == 8);
    
    EncOperand ret;
    memset(&ret, 0, sizeof(ret)); 
    
    ret.op = FE_MEM(reg_to_reg(base_reg, 8), index_scale, reg_to_reg(index_reg, 8), offset);
    ret.size = word_size;
    ret.is_imm = 0;
    
    return ret;
}
static inline EncOperand enc_mem(enum Register base_reg, int64_t offset, int word_size)
{
    return enc_mem_full(base_reg, REG_NONE, 0, offset, word_size);
}
#ifdef __cplusplus
static inline EncOperand enc_mem(uint64_t base_reg, int64_t offset, int word_size)
{
    return enc_mem((enum Register)base_reg, offset, word_size);
}
#endif

static inline EncOperand enc_mem_add_offset(EncOperand op, uint32_t offset)
{
    assert(FE_ISMEM(op));
    uint32_t _offs = op.op & 0xFFFFFFFF;
    _offs += offset;
    op.op &= 0xFFFFFFFF00000000ULL;
    op.op |= _offs;
    return op;
}

static inline EncOperand enc_mem_change_size(EncOperand op, int size)
{
    assert(FE_ISMEM(op));
    assert(size == 1 || size == 2 || size == 4 || size == 8);
    op.size = size;
    return op;
}
static inline EncOperand enc_imm(uint64_t imm, int size)
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
    
    ret.op = imm;
    ret.is_imm = 1;
    
    return ret;
}

/*
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
        default:
            return "UNKNOWN ZYDIS STATUS";
            //printf("UNKNOWN ZYDIS STATUS %d\n", status);
            //assert(0);
    };
}
*/

static inline void enc_emit_n(byte_buffer * bytes, int name, EncOperand * ops, int n)
{
    assert(n <= 5);
    
    uint8_t buf[16];
    memset(buf, 0, 16);
    uint8_t * cur = buf;
    
    printf("inst: %d\n", name);
    for (int64_t i = 0; i < n; i++)
        printf("op%zu: %zX %d\n", i, ops[i].op, ops[i].is_imm);
    
    uint64_t inst = name_to_inst(name, ops, n);
    
    // Fadec has a design flaw where the expected value for jump operands is a pointer into the encoding buffer.
    // Here we fix this.
    if (inst == FE_JMP || inst == FE_JCXZ ||
        inst == FE_JO || inst == FE_JNO || inst == FE_JC || inst == FE_JNC ||
        inst == FE_JZ || inst == FE_JNZ || inst == FE_JBE || inst == FE_JA ||
        inst == FE_JS || inst == FE_JNS || inst == FE_JP || inst == FE_JNP ||
        inst == FE_JL || inst == FE_JGE || inst == FE_JLE || inst == FE_JG)
    {
        assert(n == 1);
        uint64_t offs = (uint64_t)buf;
        if (ops[0].is_imm)
            ops[0].op += offs;
    }
    
    int failed = 0;
    
    if (n == 5)
        failed = fe_enc64(&cur, inst, ops[0].op, ops[1].op, ops[2].op, ops[3].op, ops[4].op);
    else if (n == 4)
        failed = fe_enc64(&cur, inst, ops[0].op, ops[1].op, ops[2].op, ops[3].op);
    else if (n == 3)
        failed = fe_enc64(&cur, inst, ops[0].op, ops[1].op, ops[2].op);
    else if (n == 2)
        failed = fe_enc64(&cur, inst, ops[0].op, ops[1].op);
    else if (n == 1)
        failed = fe_enc64(&cur, inst, ops[0].op);
    else if (n == 0)
        failed = fe_enc64(&cur, inst);
    
    printf("error code: %d\n", failed);
    if (failed)
    {
        assert(!failed);
    }
    
    size_t len = cur - buf;
    
    bytes_push(bytes, buf, len);
}

static inline void enc_emit_4(byte_buffer * bytes, int name, EncOperand op1, EncOperand op2, EncOperand op3, EncOperand op4)
{
    EncOperand ops[] = {op1, op2, op3, op4};
    enc_emit_n(bytes, name, ops, 4);
}
static inline void enc_emit_3(byte_buffer * bytes, int name, EncOperand op1, EncOperand op2, EncOperand op3)
{
    EncOperand ops[] = {op1, op2, op3};
    enc_emit_n(bytes, name, ops, 3);
}
static inline void enc_emit_2(byte_buffer * bytes, int name, EncOperand op1, EncOperand op2)
{
    if (name == INST_MOV && memcmp(&op1, &op2, sizeof(EncOperand)) == 0)
        return;
    EncOperand ops[] = {op1, op2};
    enc_emit_n(bytes, name, ops, 2);
}
static inline void enc_emit_1(byte_buffer * bytes, int name, EncOperand op1)
{
    EncOperand ops[] = {op1};
    enc_emit_n(bytes, name, ops, 1);
}
static inline void enc_emit_0(byte_buffer * bytes, int name)
{
    enc_emit_n(bytes, name, 0, 0);
}

static inline void enc_emit_nops(byte_buffer * bytes, int count)
{
    assert(count <= 15);
    while (count > 0)
    {
        if (count == 1)
            byte_push(bytes, 0x90);
        else if (count == 1)
        {
            uint8_t buf[] = {0x66, 0x90};
            bytes_push(bytes, buf, 2);
        }
        else if (count == 3)
        {
            uint8_t buf[] = {0x0f, 0x1f, 0x00};
            bytes_push(bytes, buf, 3);
        }
        else if (count == 4)
        {
            uint8_t buf[] = {0x0f, 0x1f, 0x40, 0x00};
            bytes_push(bytes, buf, 4);
        }
        else if (count == 5)
        {
            uint8_t buf[] = {0x0f, 0x1f, 0x44, 0x00, 0x00};
            bytes_push(bytes, buf, 5);
        }
        else if (count == 6)
        {
            uint8_t buf[] = {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00};
            bytes_push(bytes, buf, 6);
        }
        else if (count == 7)
        {
            uint8_t buf[] = {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00};
            bytes_push(bytes, buf, 7);
        }
        else if (count >= 8)
        {
            uint8_t buf[] = {0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
            bytes_push(bytes, buf, 8);
            count -= 8;
            continue;
        }
        return;
    }
}

#undef FE_ISMEM
#undef FE_ISREG
#undef FE_ISREG
#undef FE_ISGEN
#undef FE_ISSSE

#endif // _BBAE_EMITTER_H
