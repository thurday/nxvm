/* This file is a part of NXVM project. */

#include "stdarg.h"
#include "stdlib.h"
#include "string.h"

#include "../vapi.h"
#include "../vcpuins.h"
#include "../vmachine.h"
#include "debug.h"
#include "aasm.h"

/* operand size */
#define _SetOperandSize(n) (flagopr = ((vcpu.cs.seg.exec.defsize ? 4 : 2) != (n)))
/* address size of the source operand */
#define _SetAddressSize(n) (flagaddr = ((vcpu.cs.seg.exec.defsize ? 4 : 2) != (n)))

typedef enum {
	TYPE_NONE,
	TYPE_R8,
	TYPE_R16,
	TYPE_R32,
	TYPE_SREG,
	TYPE_I8,
	TYPE_I16,
	TYPE_I32,//
	TYPE_M,
	TYPE_M8,
	TYPE_M16,
	TYPE_M32,
	TYPE_I16_16,
	TYPE_LABEL,
	TYPE_CREG,//
	TYPE_DREG,//
	TYPE_TREG,//
	TYPE_I16_32,//
	TYPE_I32_16//
} t_aasm_oprtype;
typedef enum {
	MOD_M,
	MOD_M_DISP8,
	MOD_M_DISP16,
	MOD_M_DISP32,//
	MOD_R
} t_aasm_oprmod;
typedef enum {
	MEM_BX_SI, MEM_BX_DI,
	MEM_BP_SI, MEM_BP_DI,
	MEM_SI, MEM_DI,
	MEM_BP, MEM_BX,
	MEM_EAX, MEM_ECX,
	MEM_EDX, MEM_EBX,
	MEM_SIB, MEM_EBP,
	MEM_ESI, MEM_EDI
} t_aasm_oprmem;
typedef enum {
	R8_AL,
	R8_CL,
	R8_DL,
	R8_BL,
	R8_AH,
	R8_CH,
	R8_DH,
	R8_BH
} t_aasm_oprreg8;
typedef enum {
	R16_AX,
	R16_CX,
	R16_DX,
	R16_BX,
	R16_SP,
	R16_BP,
	R16_SI,
	R16_DI
} t_aasm_oprreg16;
typedef enum {
	R32_EAX,
	R32_ECX,
	R32_EDX,
	R32_EBX,
	R32_ESP,
	R32_EBP,
	R32_ESI,
	R32_EDI
} t_aasm_oprreg32;//
typedef enum {
	SREG_ES,
	SREG_CS,
	SREG_SS,
	SREG_DS,
	SREG_FS,//
	SREG_GS//
} t_aasm_oprsreg;
typedef enum {
	CREG_CR0,
	CREG_CR1,
	CREG_CR2,
	CREG_CR3,
	CREG_CR4,
	CREG_CR5,
	CREG_CR6,
	CREG_CR7
} t_aasm_oprcreg;
typedef enum {
	DREG_DR0,
	DREG_DR1,
	DREG_DR2,
	DREG_DR3,
	DREG_DR4,
	DREG_DR5,
	DREG_DR6,
	DREG_DR7
} t_aasm_oprdreg;
typedef enum {
	TREG_TR0,
	TREG_TR1,
	TREG_TR2,
	TREG_TR3,
	TREG_TR4,
	TREG_TR5,
	TREG_TR6,
	TREG_TR7
} t_aasm_oprtreg;
typedef enum {
	PTR_NONE,
	PTR_SHORT,
	PTR_NEAR,
	PTR_FAR
} t_aasm_oprptr;
typedef struct {
	t_aasm_oprreg32 base;
	t_aasm_oprreg32 index;
	t_nubit8 scale;
} t_aasm_oprsib;
typedef struct {
	t_aasm_oprtype  type;
	t_aasm_oprmod   mod;   // active when type = 1,2,3,6,7,8
	                       // 0 = mem; 1 = mem+disp8; 2 = mem+disp16; 3 = reg
	t_aasm_oprmem   mem;   // active when mod = 0,1,2
                           // 0 = [BX+SI], 1 = [BX+DI], 2 = [BP+SI], 3 = [BP+DI],
                           // 4 = [SI], 5 = [DI], 6 = [BP], 7 = [BX]
	t_aasm_oprsib   sib;   // active when mem = MEM_SIB
	t_aasm_oprreg8  reg8;  // active when type = 1, mod = 3
                           // 0 = AL, 1 = CL, 2 = DL, 3 = BL,
                           // 4 = AH, 5 = CH, 6 = DH, 7 = BH
	t_aasm_oprreg16 reg16; // active when type = 2, mod = 3
                           // 0 = AX, 1 = CX, 2 = DX, 3 = BX,
                           // 4 = SP, 5 = BP, 6 = SI, 7 = DI
	t_aasm_oprreg32 reg32; // active when type = 3, mod = 3
                           // 0 = EAX, 1 = ECX, 2 = EDX, 3 = EBX,
                           // 4 = ESP, 5 = EBP, 6 = ESI, 7 = EDI   
	t_aasm_oprsreg  sreg;   // active when type = 3
                           // 0 = ES, 1 = CS, 2 = SS, 3 = DS, 4 = FS, 5 = GS
	t_aasm_oprcreg  creg;
	t_aasm_oprdreg  dreg;
	t_aasm_oprtreg  treg;
	t_bool          imms;  // if imm is signed
	t_bool          immn;  // if imm is negative
	t_nubit8        imm8;
	t_nubit16       imm16;
	t_nubit32       imm32;
	t_nsbit8        disp8;
	t_nubit16       disp16;// use as imm when type = 6; use by modrm as disp when mod = 0,1,2;
	t_nubit32       disp32;// use as imm when type = 7; use by modrm as disp when mod = 0,1,2;
	t_aasm_oprptr   ptr; // 0 = near; 1 = far
	t_nubit16       pcs,pip;
	char            label[0x100];
	t_bool flages, flagcs, flagss, flagds, flagfs, flaggs;
} t_aasm_oprinfo;
/* global variables */
static t_bool flagopr, flagaddr;
static t_bool flaglock, flagrepz, flagrepnz;
static t_nubit8 acode[15];
static t_nubit8 iop;
static t_strptr rop, ropr1, ropr2, ropr3;
static t_nubit16 avcs, avip;
static t_strptr aop, aopr1, aopr2;
static t_bool flagerror;
static t_aasm_oprinfo aopri1, aopri2, aopri3;
static t_aasm_oprinfo *rinfo = NULL;
/* arg flag level 0 */
#define isNONE(oprinf)  ((oprinf).type  == TYPE_NONE)
#define isR8(oprinf)    ((oprinf).type  == TYPE_R8 && (oprinf).mod == MOD_R)
#define isR16(oprinf)   ((oprinf).type  == TYPE_R16 && (oprinf).mod == MOD_R)
#define isR32(oprinf)   ((oprinf).type  == TYPE_R32 && (oprinf).mod == MOD_R)
#define isSREG(oprinf)  ((oprinf).type  == TYPE_SREG && (oprinf).mod == MOD_R)
#define isI8(oprinf)    ((oprinf).type  == TYPE_I8)
#define isI8u(oprinf)   (isI8(oprinf)   && !(oprinf).imms)
#define isI8s(oprinf)   (isI8(oprinf)   && (oprinf).imms)
#define isI16(oprinf)   ((oprinf).type  == TYPE_I16)
#define isI16u(oprinf)  (isI16(oprinf)  && !(oprinf).imms)
#define isI16s(oprinf)  (isI16(oprinf)  && (oprinf).imms)
#define isI32(oprinf)   ((oprinf).type  == TYPE_I32)
#define isI32u(oprinf)  (isI32(oprinf)  && !(oprinf).imms)
#define isI32s(oprinf)  (isI32(oprinf)  && (oprinf).imms)
#define isI16p(oprinf)  ((oprinf).type  == TYPE_I16_16)
#define isM(oprinf)     (((oprinf).type == TYPE_M   || (oprinf).type == TYPE_M8   || \
                          (oprinf).type == TYPE_M16 || (oprinf).type == TYPE_M32) && (oprinf).mod != MOD_R)
#define isM8(oprinf)    (((oprinf).type == TYPE_M   || (oprinf).type == TYPE_M8 ) && (oprinf).mod != MOD_R)
#define isM16(oprinf)   (((oprinf).type == TYPE_M   || (oprinf).type == TYPE_M16) && (oprinf).mod != MOD_R)
#define isM32(oprinf)   (((oprinf).type == TYPE_M   || (oprinf).type == TYPE_M32) && (oprinf).mod != MOD_R)
#define isMs(oprinf)    ((oprinf).type  == TYPE_M   && (oprinf).mod != MOD_R)
#define isM8s(oprinf)   ((oprinf).type  == TYPE_M8  && (oprinf).mod != MOD_R)
#define isM16s(oprinf)  ((oprinf).type  == TYPE_M16 && (oprinf).mod != MOD_R)
#define isM32s(oprinf)  ((oprinf).type  == TYPE_M32 && (oprinf).mod != MOD_R)
#define isLABEL(oprinf) ((oprinf).type  == TYPE_LABEL)
#define isPNONE(oprinf) ((oprinf).ptr == PTR_NONE)
#define isNEAR(oprinf)  ((oprinf).ptr == PTR_NEAR)
#define isSHORT(oprinf) ((oprinf).ptr == PTR_SHORT)
#define isFAR(oprinf)   ((oprinf).ptr == PTR_FAR)
/* arg flag level 1 */
#define isRM8s(oprinf)  (isR8 (oprinf) || isM8s(oprinf))
#define isRM16s(oprinf) (isR16(oprinf) || isM16s(oprinf))
#define isRM32s(oprinf) (isR32(oprinf) || isM32s(oprinf))
#define isRM8(oprinf)   (isR8 (oprinf) || isM8 (oprinf))
#define isRM16(oprinf)  (isR16(oprinf) || isM16(oprinf))
#define isRM32(oprinf)  (isR32(oprinf) || isM32(oprinf))
#define isRM(oprinf)    (isRM8(oprinf) || isRM16(oprinf))
#define isAL(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_AL)
#define isCL(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_CL)
#define isDL(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_DL)
#define isBL(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_BL)
#define isAH(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_AH)
#define isCH(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_CH)
#define isDH(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_DH)
#define isBH(oprinf)    (isR8 (oprinf) && (oprinf).reg8  == R8_BH)
#define isAX(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_AX)
#define isCX(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_CX)
#define isDX(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_DX)
#define isBX(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_BX)
#define isSP(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_SP)
#define isBP(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_BP)
#define isSI(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_SI)
#define isDI(oprinf)    (isR16(oprinf) && (oprinf).reg16 == R16_DI)
#define isEAX(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_EAX)
#define isECX(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_ECX)
#define isEDX(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_EDX)
#define isEBX(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_EBX)
#define isESP(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_ESP)
#define isEBP(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_EBP)
#define isESI(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_ESI)
#define isEDI(oprinf)   (isR32(oprinf) && (oprinf).reg32 == R32_EDI)
#define isES(oprinf)    (isSREG(oprinf) && (oprinf).sreg   == SREG_ES)
#define isCS(oprinf)    (isSREG(oprinf) && (oprinf).sreg   == SREG_CS)
#define isSS(oprinf)    (isSREG(oprinf) && (oprinf).sreg   == SREG_SS)
#define isDS(oprinf)    (isSREG(oprinf) && (oprinf).sreg   == SREG_DS)
#define isFS(oprinf)    (isSREG(oprinf) && (oprinf).sreg   == SREG_FS)
#define isGS(oprinf)    (isSREG(oprinf) && (oprinf).sreg   == SREG_GS)
#define isESDI8(oprinf)  ( isM8 (oprinf) && (oprinf).flages && (oprinf).mem == MEM_DI  && (oprinf).mod == MOD_M)
#define isESDI16(oprinf)  (isM16(oprinf) && (oprinf).flages && (oprinf).mem == MEM_DI  && (oprinf).mod == MOD_M)
#define isESDI32(oprinf)  (isM32(oprinf) && (oprinf).flages && (oprinf).mem == MEM_DI  && (oprinf).mod == MOD_M)
#define isESEDI8(oprinf)  (isM8 (oprinf) && (oprinf).flages && (oprinf).mem == MEM_EDI && (oprinf).mod == MOD_M)
#define isESEDI16(oprinf) (isM16(oprinf) && (oprinf).flages && (oprinf).mem == MEM_EDI && (oprinf).mod == MOD_M)
#define isESEDI32(oprinf) (isM32(oprinf) && (oprinf).flages && (oprinf).mem == MEM_EDI && (oprinf).mod == MOD_M)
#define isDSSI8(oprinf)  ( isM8 (oprinf) && (oprinf).mem == MEM_SI  && (oprinf).mod == MOD_M)
#define isDSSI16(oprinf)  (isM16(oprinf) && (oprinf).mem == MEM_SI  && (oprinf).mod == MOD_M)
#define isDSSI32(oprinf)  (isM32(oprinf) && (oprinf).mem == MEM_SI  && (oprinf).mod == MOD_M)
#define isDSESI8(oprinf)  (isM8 (oprinf) && (oprinf).mem == MEM_ESI && (oprinf).mod == MOD_M)
#define isDSESI16(oprinf) (isM16(oprinf) && (oprinf).mem == MEM_ESI && (oprinf).mod == MOD_M)
#define isDSESI32(oprinf) (isM32(oprinf) && (oprinf).mem == MEM_ESI && (oprinf).mod == MOD_M)
/* arg flag level 2 */
#define ARG_NONE        (isNONE (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_I8          (isI8   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_I16         (isI16  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_I32         (isI32  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_I8s         (isI8s  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_I16s        (isI16s (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_I32s        (isI32s (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_R32         (isR32  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_M32         (isM32  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_M16s        (isM16s (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_M32s        (isM32s (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_RM8s        (isRM8s (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_RM16s       (isRM16s(aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_RM32s       (isRM32s(aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_RM8_R8      (isRM8  (aopri1) && isR8  (aopri2)  && isNONE(aopri3))
#define ARG_RM16_R16    (isRM16 (aopri1) && isR16 (aopri2)  && isNONE(aopri3))
#define ARG_RM32_R32    (isRM32 (aopri1) && isR32 (aopri2)  && isNONE(aopri3))
#define ARG_R8_RM8      (isR8   (aopri1) && isRM8 (aopri2)  && isNONE(aopri3))
#define ARG_R16_RM16    (isR16  (aopri1) && isRM16(aopri2)  && isNONE(aopri3))
#define ARG_R32_RM32    (isR32  (aopri1) && isRM32(aopri2)  && isNONE(aopri3))
#define ARG_R16_RM8     (isR16 (aopri1)  && isRM8s(aopri2)  && isNONE(aopri3))
#define ARG_R32_RM8     (isR32 (aopri1)  && isRM8s(aopri2)  && isNONE(aopri3))
#define ARG_R32_RM16    (isR32 (aopri1)  && isRM16s(aopri2) && isNONE(aopri3))
#define ARG_ES          (isES   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_CS          (isCS   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_SS          (isSS   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_DS          (isDS   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_FS          (isFS   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_GS          (isGS   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_AX          (isAX   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_CX          (isCX   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_DX          (isDX   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_BX          (isBX   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_SP          (isSP   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_BP          (isBP   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_SI          (isSI   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_DI          (isDI   (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_EAX         (isEAX  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_ECX         (isECX  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_EDX         (isEDX  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_EBX         (isEBX  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_ESP         (isESP  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_EBP         (isEBP  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_ESI         (isESI  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_EDI         (isEDI  (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_AL_I8u      (isAL   (aopri1) && isI8u (aopri2)  && isNONE(aopri3))
#define ARG_AX_I8u      (isAX   (aopri1) && isI8u (aopri2)  && isNONE(aopri3))
#define ARG_I8u_AL      (isI8u  (aopri1) && isAL  (aopri2)  && isNONE(aopri3))
#define ARG_I8u_AX      (isI8u  (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_AL_I8       (isAL   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_CL_I8       (isCL   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_DL_I8       (isDL   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_BL_I8       (isBL   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_AH_I8       (isAH   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_CH_I8       (isCH   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_DH_I8       (isDH   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_BH_I8       (isBH   (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_AX_I16      (isAX   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_CX_I16      (isCX   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_DX_I16      (isDX   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_BX_I16      (isBX   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_SP_I16      (isSP   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_BP_I16      (isBP   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_SI_I16      (isSI   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_DI_I16      (isDI   (aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_EAX_I32     (isEAX  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_ECX_I32     (isECX  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_EDX_I32     (isEDX  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_EBX_I32     (isEBX  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_ESP_I32     (isESP  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_EBP_I32     (isEBP  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_ESI_I32     (isESI  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_EDI_I32     (isEDI  (aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_RM8_I8      (isRM8s (aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_RM16_I16    (isRM16s(aopri1) && isI16 (aopri2)  && isNONE(aopri3))
#define ARG_RM32_I32    (isRM32s(aopri1) && isI32 (aopri2)  && isNONE(aopri3))
#define ARG_RM16_I8     (isRM16s(aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_RM32_I8     (isRM32s(aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_RM16_I8     (isRM16s(aopri1) && isI8  (aopri2)  && isNONE(aopri3))
#define ARG_RM16_SREG   (isRM16 (aopri1) && isSREG (aopri2) && isNONE(aopri3))
#define ARG_SREG_RM16   (isSREG  (aopri1) && isRM16(aopri2) && isNONE(aopri3))
#define ARG_RM16        (isRM16 (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_RM32        (isRM32 (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_AX_AX       (isAX   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_CX_AX       (isCX   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_DX_AX       (isDX   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_BX_AX       (isBX   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_SP_AX       (isSP   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_BP_AX       (isBP   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_SI_AX       (isSI   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_DI_AX       (isDI   (aopri1) && isAX  (aopri2)  && isNONE(aopri3))
#define ARG_AL_M8       (isAL   (aopri1) && isM8  (aopri2)  && (aopri2.mod == MOD_M && (aopri2.mem == MEM_BP || aopri2.mem == MEM_EBP)) && isNONE(aopri3))
#define ARG_M8_AL       (isM8   (aopri1) && isAL  (aopri2)  && (aopri1.mod == MOD_M && (aopri1.mem == MEM_BP || aopri2.mem == MEM_EBP)) && isNONE(aopri3))
#define ARG_AX_M16      (isAX   (aopri1) && isM16 (aopri2)  && (aopri2.mod == MOD_M && (aopri2.mem == MEM_BP || aopri2.mem == MEM_EBP)) && isNONE(aopri3))
#define ARG_M16_AX      (isM16  (aopri1) && isAX  (aopri2)  && (aopri1.mod == MOD_M && (aopri1.mem == MEM_BP || aopri1.mem == MEM_EBP)) && isNONE(aopri3))
#define ARG_EAX_M32     (isEAX  (aopri1) && isM32 (aopri2)  && (aopri2.mod == MOD_M && (aopri2.mem == MEM_BP || aopri2.mem == MEM_EBP)) && isNONE(aopri3))
#define ARG_M32_EAX     (isM32  (aopri1) && isEAX (aopri2)  && (aopri1.mod == MOD_M && (aopri1.mem == MEM_BP || aopri1.mem == MEM_EBP)) && isNONE(aopri3))
#define ARG_R16_M16     (isR16  (aopri1) && isM16 (aopri2)  && isNONE(aopri3))
#define ARG_I16u        (isI16u (aopri1) && isNONE(aopri2)  && isNONE(aopri3))
#define ARG_PNONE_I8s   (isPNONE(aopri1) && isI8s (aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_PNONE_I16s  (isPNONE(aopri1) && isI16s(aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_SHORT_I8s   (isSHORT(aopri1) && isI8s (aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_NEAR_I16s   (isNEAR (aopri1) && isI16s(aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_PNONE_I16   (isPNONE(aopri1) && isI16u(aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_SHORT_I16   (isSHORT(aopri1) && isI16u(aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_NEAR_I16    (isNEAR (aopri1) && isI16u(aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_FAR_I16_16  (isFAR  (aopri1) && isI16p(aopri1)  && isNONE(aopri2) && isNONE(aopri3))
#define ARG_PNONE_LABEL (isPNONE(aopri1) && isLABEL(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_SHORT_LABEL (isSHORT(aopri1) && isLABEL(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_NEAR_LABEL  (isNEAR (aopri1) && isLABEL(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_FAR_LABEL   (isFAR  (aopri1) && isLABEL(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_PNONE_RM16s (isPNONE(aopri1) && isRM16s(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_NEAR_RM16s  (isNEAR (aopri1) && isRM16s(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_PNONE_RM32s (isPNONE(aopri1) && isRM32s(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_NEAR_RM32s  (isNEAR (aopri1) && isRM32s(aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_FAR_M16_16  (isFAR  (aopri1) && isM32  (aopri1) && isNONE(aopri2) && isNONE(aopri3))
#define ARG_RM8_CL      (isRM8s (aopri1) && isCL   (aopri2) && isNONE(aopri3))
#define ARG_RM16_CL     (isRM16s(aopri1) && isCL   (aopri2) && isNONE(aopri3))
#define ARG_AL_DX       (isAL   (aopri1) && isDX   (aopri2) && isNONE(aopri3))
#define ARG_DX_AL       (isDX   (aopri1) && isAL   (aopri2) && isNONE(aopri3))
#define ARG_AX_DX       (isAX   (aopri1) && isDX   (aopri2) && isNONE(aopri3))
#define ARG_DX_AX       (isDX   (aopri1) && isAX   (aopri2) && isNONE(aopri3))
#define ARG_ESDI8_DSSI8   (isESDI8 (aopri1) && isDSSI8 (aopri2) && isNONE(aopri3))
#define ARG_ESDI16_DSSI16 (isESDI16(aopri1) && isDSSI16(aopri2) && isNONE(aopri3))
#define ARG_ESDI32_DSSI32 (isESDI32(aopri1) && isDSSI32(aopri2) && isNONE(aopri3))
#define ARG_DSSI8_ESDI8   (isDSSI8 (aopri1) && isESDI8 (aopri2) && isNONE(aopri3))
#define ARG_DSSI16_ESDI16 (isDSSI16(aopri1) && isESDI16(aopri2) && isNONE(aopri3))
#define ARG_DSSI32_ESDI32 (isDSSI32(aopri1) && isESDI32(aopri2) && isNONE(aopri3))
#define ARG_ESDI8_DX      (isESDI8 (aopri1) && isDX    (aopri2) && isNONE(aopri3))
#define ARG_ESDI16_DX     (isESDI16(aopri1) && isDX    (aopri2) && isNONE(aopri3))
#define ARG_ESDI32_DX     (isESDI32(aopri1) && isDX    (aopri2) && isNONE(aopri3))
#define ARG_DX_DSSI8      (isDX(aopri1)     && isDSSI8 (aopri2) && isNONE(aopri3))
#define ARG_DX_DSSI16     (isDX(aopri1)     && isDSSI16(aopri2) && isNONE(aopri3))
#define ARG_DX_DSSI32     (isDX(aopri1)     && isDSSI32(aopri2) && isNONE(aopri3))
#define ARG_DSSI8         (isDSSI8 (aopri1) && isNONE  (aopri2) && isNONE(aopri3))
#define ARG_DSSI16        (isDSSI16(aopri1) && isNONE  (aopri2) && isNONE(aopri3))
#define ARG_DSSI32        (isDSSI32(aopri1) && isNONE  (aopri2) && isNONE(aopri3))
#define ARG_ESDI8         (isESDI8 (aopri1) && isNONE  (aopri2) && isNONE(aopri3))
#define ARG_ESDI16        (isESDI16(aopri1) && isNONE  (aopri2) && isNONE(aopri3))
#define ARG_ESDI32        (isESDI32(aopri1) && isNONE  (aopri2) && isNONE(aopri3))
#define ARG_ESEDI8_DSESI8   (isESEDI8 (aopri1) && isDSESI8 (aopri2) && isNONE(aopri3))
#define ARG_ESEDI16_DSESI16 (isESEDI16(aopri1) && isDSESI16(aopri2) && isNONE(aopri3))
#define ARG_ESEDI32_DSESI32 (isESEDI32(aopri1) && isDSESI32(aopri2) && isNONE(aopri3))
#define ARG_DSESI8_ESEDI8   (isDSESI8 (aopri1) && isESEDI8 (aopri2) && isNONE(aopri3))
#define ARG_DSESI16_ESEDI16 (isDSESI16(aopri1) && isESEDI16(aopri2) && isNONE(aopri3))
#define ARG_DSESI32_ESEDI32 (isDSESI32(aopri1) && isESEDI32(aopri2) && isNONE(aopri3))
#define ARG_ESEDI8_DX       (isESEDI8 (aopri1) && isDX    (aopri2)  && isNONE(aopri3))
#define ARG_ESEDI16_DX      (isESEDI16(aopri1) && isDX    (aopri2)  && isNONE(aopri3))
#define ARG_ESEDI32_DX      (isESEDI32(aopri1) && isDX    (aopri2)  && isNONE(aopri3))
#define ARG_DX_DSESI8       (isDX(aopri1)      && isDSESI8 (aopri2) && isNONE(aopri3))
#define ARG_DX_DSESI16      (isDX(aopri1)      && isDSESI16(aopri2) && isNONE(aopri3))
#define ARG_DX_DSESI32      (isDX(aopri1)      && isDSESI32(aopri2) && isNONE(aopri3))
#define ARG_DSESI8          (isDSESI8 (aopri1) && isNONE  (aopri2)  && isNONE(aopri3))
#define ARG_DSESI16         (isDSESI16(aopri1) && isNONE  (aopri2)  && isNONE(aopri3))
#define ARG_DSESI32         (isDSESI32(aopri1) && isNONE  (aopri2)  && isNONE(aopri3))
#define ARG_ESEDI8          (isESEDI8 (aopri1) && isNONE  (aopri2)  && isNONE(aopri3))
#define ARG_ESEDI16         (isESEDI16(aopri1) && isNONE  (aopri2)  && isNONE(aopri3))
#define ARG_ESEDI32         (isESEDI32(aopri1) && isNONE  (aopri2)  && isNONE(aopri3))
/* assembly compiler: lexical scanner */
typedef enum {
	STATE_START,
	        STATE_BY,STATE_BYT, /* BYTE */
	STATE_W,STATE_WO,STATE_WOR, /* WORD */
	        STATE_DW,STATE_DWO, /* DWORD */
	STATE_DWOR,
	STATE_P,STATE_PT,           /* PTR */
	STATE_N,STATE_NE,STATE_NEA, /* NEAR */
	        STATE_EA,           /* EAX */
	        STATE_EC,           /* ECX */
	        STATE_ED,           /* EDX, EDI */
	        STATE_EB,           /* EBX, EBP */
	        STATE_ES,           /* ESP, ESI */
	        STATE_FA,           /* FAR */
	        STATE_SH,STATE_SHO, STATE_SHOR, /* SHORT */
			STATE_CR,           /* CRn */
			STATE_DR,           /* DRn */
	STATE_T,STATE_TR,           /* TRn */
	STATE_A,                    /* AX, AH, AL, NUM */
	STATE_B,                    /* BX, BH, BL, BP, NUM */
	STATE_C,                    /* CX, CH, CL, CS, NUM */
	STATE_D,                    /* DX, DH, DL, DS, DI, NUM, DWORD */
	STATE_E,                    /* ES, NUM */
	STATE_F,                    /* FS, NUM, FAR */
	STATE_G,                    /* GS, NUM, FAR */
	STATE_S,                    /* SS, SP, SI, SHORT */
	STATE_NUM1,                 /* NUM */
	STATE_NUM2,
	STATE_NUM3,
	STATE_NUM4,
	STATE_NUM5,
	STATE_NUM6,
	STATE_NUM7,
	STATE_NUM8
} t_aasm_scan_state;
typedef enum {
	TOKEN_NULL,TOKEN_END,
	TOKEN_LSPAREN,TOKEN_RSPAREN,
	TOKEN_COLON,TOKEN_PLUS,TOKEN_MINUS,TOKEN_TIMES,
	TOKEN_BYTE,TOKEN_WORD,TOKEN_DWORD,
	TOKEN_SHORT,TOKEN_NEAR,TOKEN_FAR,TOKEN_PTR,
	TOKEN_IMM8,TOKEN_IMM16,TOKEN_IMM32,
	TOKEN_AH,TOKEN_BH,TOKEN_CH,TOKEN_DH,
	TOKEN_AL,TOKEN_BL,TOKEN_CL,TOKEN_DL,
	TOKEN_AX,TOKEN_BX,TOKEN_CX,TOKEN_DX,
	TOKEN_SP,TOKEN_BP,TOKEN_SI,TOKEN_DI,
	TOKEN_EAX,TOKEN_EBX,TOKEN_ECX,TOKEN_EDX,
	TOKEN_ESP,TOKEN_EBP,TOKEN_ESI,TOKEN_EDI,
	TOKEN_ES,TOKEN_CS,TOKEN_SS,TOKEN_DS,TOKEN_FS,TOKEN_GS,
	TOKEN_CR0,TOKEN_CR2,TOKEN_CR3,
	TOKEN_DR0,TOKEN_DR1,TOKEN_DR2,TOKEN_DR3,TOKEN_DR6,TOKEN_DR7,
	TOKEN_TR6,TOKEN_TR7,
	TOKEN_CHAR,TOKEN_STRING,TOKEN_LABEL
} t_aasm_token;
/* token variables */
static t_nubit8 tokimm8;
static t_nubit16 tokimm16;
static t_nubit32 tokimm32;
static t_nsbit8 tokchar;
t_string tokstring, toklabel;
#define tokch  (*tokptr)
#define take(n) (flagend = 1, token = (n))
static t_aasm_token gettoken(t_strptr str)
{
	static t_strptr tokptr = NULL;
	t_nubit8 i;
	t_nubit8 toklen = 0;
	t_nubit32 tokimm = 0;
	t_bool flagend = 0;
	t_aasm_token token = TOKEN_NULL;
	t_aasm_scan_state state = STATE_START;
	t_strptr tokptrbak;
	tokimm8 = 0x00;
	tokimm16 = 0x0000;
	tokimm32 = 0x00000000;
	if (str) tokptr = str;
	if (!tokptr) return token;
	tokptrbak = tokptr;
	do {
		switch (state) {
		case STATE_START:
			switch (tokch) {
			case '[': take(TOKEN_LSPAREN);break;
			case ']': take(TOKEN_RSPAREN);break;
			case ':': take(TOKEN_COLON);   break;
			case '+': take(TOKEN_PLUS);    break;
			case '-': take(TOKEN_MINUS);   break;
			case '*': take(TOKEN_TIMES);   break;
			case '0': tokimm = 0x0;toklen = 1;state = STATE_NUM1;break;
			case '1': tokimm = 0x1;toklen = 1;state = STATE_NUM1;break;
			case '2': tokimm = 0x2;toklen = 1;state = STATE_NUM1;break;
			case '3': tokimm = 0x3;toklen = 1;state = STATE_NUM1;break;
			case '4': tokimm = 0x4;toklen = 1;state = STATE_NUM1;break;
			case '5': tokimm = 0x5;toklen = 1;state = STATE_NUM1;break;
			case '6': tokimm = 0x6;toklen = 1;state = STATE_NUM1;break;
			case '7': tokimm = 0x7;toklen = 1;state = STATE_NUM1;break;
			case '8': tokimm = 0x8;toklen = 1;state = STATE_NUM1;break;
			case '9': tokimm = 0x9;toklen = 1;state = STATE_NUM1;break;
			case 'a': tokimm = 0xa;toklen = 1;state = STATE_A;break;
			case 'b': tokimm = 0xb;toklen = 1;state = STATE_B;break;
			case 'c': tokimm = 0xc;toklen = 1;state = STATE_C;break;
			case 'd': tokimm = 0xd;toklen = 1;state = STATE_D;break;
			case 'e': tokimm = 0xe;toklen = 1;state = STATE_E;break;
			case 'f': tokimm = 0xf;toklen = 1;state = STATE_F;break;
			case 'n': state = STATE_N;break;
			case 'p': state = STATE_P;break;
			case 's': state = STATE_S;break;
			case 't': state = STATE_T;break;
			case 'w': state = STATE_W;break;
			case '\'':
				if (*(tokptr + 2) == '\'') {
					tokptr++;
					tokchar = tokch;
					tokptr++;
					take(TOKEN_CHAR);
				} else if (*(tokptr + 1) == '\'') {
					tokptr++;
					tokchar = 0x00;
					take(TOKEN_CHAR);
				} else {
					tokptr--;
					flagerror = 1;
					take(TOKEN_NULL);
				}
				break;
			case '\"':
				tokstring[0] = tokstring[0xff] = 0x00;
				i = 0;
				do {
					tokptr++;
					tokstring[i++] = tokch;
				} while (tokch && (tokch != '\"'));
				if (!tokch) {
					tokptr = tokptrbak;
					flagerror = 1;
					take(TOKEN_NULL);
				} else {
					tokstring[i - 1] = 0x00;
					take(TOKEN_STRING);
				}
				break;
			case '$':
				tokptr++;
				if (tokch != '(') {
					tokptr -= 2;
					flagerror = 1;
					take(TOKEN_NULL);
				} else {
					toklabel[0] =toklabel[0xff] = 0x00;
					i = 0;
					do {
						tokptr++;
						toklabel[i++] = tokch;
					} while (tokch && tokch != ')');
					if (!tokch) {
						tokptr = tokptrbak;
						flagerror = 1;
						take(TOKEN_NULL);
					} else {
						toklabel[i - 1] = 0x00;
						take(TOKEN_LABEL);
					}
				}
				break;
			case ' ':
			case '\t': break;
			case '\0': tokptr--;take(TOKEN_END);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NUM1:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_NUM2;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_NUM2;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_NUM2;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_NUM2;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NUM2:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 3;state = STATE_NUM3;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 3;state = STATE_NUM3;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 3;state = STATE_NUM3;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 3;state = STATE_NUM3;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 3;state = STATE_NUM3;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 3;state = STATE_NUM3;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 3;state = STATE_NUM3;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 3;state = STATE_NUM3;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 3;state = STATE_NUM3;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 3;state = STATE_NUM3;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 3;state = STATE_NUM3;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 3;state = STATE_NUM3;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 3;state = STATE_NUM3;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 3;state = STATE_NUM3;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 3;state = STATE_NUM3;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 3;state = STATE_NUM3;break;
			default: tokptr--;tokimm8 = GetMax8(tokimm);take(TOKEN_IMM8);break;
			}
			break;
		case STATE_NUM3:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 4;state = STATE_NUM4;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 4;state = STATE_NUM4;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 4;state = STATE_NUM4;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 4;state = STATE_NUM4;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 4;state = STATE_NUM4;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 4;state = STATE_NUM4;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 4;state = STATE_NUM4;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 4;state = STATE_NUM4;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 4;state = STATE_NUM4;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 4;state = STATE_NUM4;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 4;state = STATE_NUM4;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 4;state = STATE_NUM4;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 4;state = STATE_NUM4;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 4;state = STATE_NUM4;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 4;state = STATE_NUM4;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 4;state = STATE_NUM4;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NUM4:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 5;state = STATE_NUM5;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 5;state = STATE_NUM5;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 5;state = STATE_NUM5;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 5;state = STATE_NUM5;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 5;state = STATE_NUM5;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 5;state = STATE_NUM5;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 5;state = STATE_NUM5;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 5;state = STATE_NUM5;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 5;state = STATE_NUM5;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 5;state = STATE_NUM5;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 5;state = STATE_NUM5;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 5;state = STATE_NUM5;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 5;state = STATE_NUM5;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 5;state = STATE_NUM5;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 5;state = STATE_NUM5;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 5;state = STATE_NUM5;break;
			default: tokptr--;tokimm16 = GetMax16(tokimm);take(TOKEN_IMM16);break;
			}
			break;
		case STATE_NUM5:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 6;state = STATE_NUM6;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 6;state = STATE_NUM6;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 6;state = STATE_NUM6;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 6;state = STATE_NUM6;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 6;state = STATE_NUM6;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 6;state = STATE_NUM6;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 6;state = STATE_NUM6;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 6;state = STATE_NUM6;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 6;state = STATE_NUM6;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 6;state = STATE_NUM6;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 6;state = STATE_NUM6;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 6;state = STATE_NUM6;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 6;state = STATE_NUM6;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 6;state = STATE_NUM6;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 6;state = STATE_NUM6;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 6;state = STATE_NUM6;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NUM6:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 7;state = STATE_NUM7;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 7;state = STATE_NUM7;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 7;state = STATE_NUM7;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 7;state = STATE_NUM7;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 7;state = STATE_NUM7;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 7;state = STATE_NUM7;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 7;state = STATE_NUM7;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 7;state = STATE_NUM7;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 7;state = STATE_NUM7;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 7;state = STATE_NUM7;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 7;state = STATE_NUM7;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 7;state = STATE_NUM7;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 7;state = STATE_NUM7;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 7;state = STATE_NUM7;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 7;state = STATE_NUM7;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 7;state = STATE_NUM7;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NUM7:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 8;state = STATE_NUM8;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 8;state = STATE_NUM8;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 8;state = STATE_NUM8;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 8;state = STATE_NUM8;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 8;state = STATE_NUM8;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 8;state = STATE_NUM8;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 8;state = STATE_NUM8;break;
			case '7': tokimm = (tokimm << 4) | 0x8;toklen = 8;state = STATE_NUM8;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 8;state = STATE_NUM8;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 8;state = STATE_NUM8;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 8;state = STATE_NUM8;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 8;state = STATE_NUM8;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 8;state = STATE_NUM8;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 8;state = STATE_NUM8;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 8;state = STATE_NUM8;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 8;state = STATE_NUM8;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NUM8:
			switch (tokch) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				tokptr--;flagerror = 1;take(TOKEN_NULL);break;
				break;
			default: tokptr--;tokimm32 = GetMax32(tokimm);take(TOKEN_IMM32);break;
			}
			break;
		case STATE_A:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_NUM2;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_NUM2;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_NUM2;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_NUM2;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			case 'x': take(TOKEN_AX);break;
			case 'h': take(TOKEN_AH);break;
			case 'l': take(TOKEN_AL);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_B:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_NUM2;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_NUM2;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_NUM2;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_NUM2;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			case 'x': take(TOKEN_BX);break;
			case 'h': take(TOKEN_BH);break;
			case 'l': take(TOKEN_BL);break;
			case 'p': take(TOKEN_BP);break;
			case 'y': state = STATE_BY;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_C:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_NUM2;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_NUM2;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_NUM2;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_NUM2;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			case 'x': take(TOKEN_CX);break;
			case 'h': take(TOKEN_CH);break;
			case 'l': take(TOKEN_CL);break;
			case 'r': state = STATE_CR;break;
			case 's': take(TOKEN_CS);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_D:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_NUM2;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_NUM2;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_NUM2;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_NUM2;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			case 'x': take(TOKEN_DX);break;
			case 'h': take(TOKEN_DH);break;
			case 'l': take(TOKEN_DL);break;
			case 'r': state = STATE_DR;break;
			case 's': take(TOKEN_DS);break;
			case 'i': take(TOKEN_DI);break;
			case 'w': state = STATE_DW;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_E:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_EA;  break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_EB;  break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_EC;  break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_ED;  break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			case 's': state = STATE_ES;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_F:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 2;state = STATE_NUM2;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 2;state = STATE_NUM2;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 2;state = STATE_NUM2;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 2;state = STATE_NUM2;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 2;state = STATE_NUM2;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 2;state = STATE_NUM2;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 2;state = STATE_NUM2;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 2;state = STATE_NUM2;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 2;state = STATE_NUM2;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 2;state = STATE_NUM2;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 2;state = STATE_FA;  break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 2;state = STATE_NUM2;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 2;state = STATE_NUM2;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 2;state = STATE_NUM2;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 2;state = STATE_NUM2;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 2;state = STATE_NUM2;break;
			case 's': take(TOKEN_FS);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_G:
			switch (tokch) {
			case 's': take(TOKEN_GS);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_N:
			switch (tokch) {
			case 'e': state = STATE_NE;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_P:
			switch (tokch) {
			case 't': state = STATE_PT;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_S:
			switch (tokch) {
			case 'i': take(TOKEN_SI);break;
			case 'p': take(TOKEN_SP);break;
			case 's': take(TOKEN_SS);break;
			case 'h': state = STATE_SH;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_T:
			switch (tokch) {
			case 'r': state = STATE_TR;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_W:
			switch (tokch) {
			case 'o': state = STATE_WO;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_BY:
			switch (tokch) {
			case 't': state = STATE_BYT;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_CR:
			switch (tokch) {
			case '0': take(TOKEN_CR0);break;
			case '2': take(TOKEN_CR2);break;
			case '3': take(TOKEN_CR3);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_DR:
			switch (tokch) {
			case '0': take(TOKEN_DR0);break;
			case '1': take(TOKEN_DR1);break;
			case '2': take(TOKEN_DR2);break;
			case '3': take(TOKEN_DR3);break;
			case '6': take(TOKEN_DR6);break;
			case '7': take(TOKEN_DR7);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_DW:
			switch (tokch) {
			case 'o': state = STATE_DWO;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_EA:
			switch (tokch) {
			case 'x': take(TOKEN_EAX);break;
			default: tokptr--;tokimm8 = GetMax8(tokimm);take(TOKEN_IMM8);break;
			}
			break;
		case STATE_EB:
			switch (tokch) {
			case 'p': take(TOKEN_EBP);break;
			case 'x': take(TOKEN_EBX);break;
			default: tokptr--;tokimm8 = GetMax8(tokimm);take(TOKEN_IMM8);break;
			}
			break;
		case STATE_EC:
			switch (tokch) {
			case 'x': take(TOKEN_ECX);break;
			default: tokptr--;tokimm8 = GetMax8(tokimm);take(TOKEN_IMM8);break;
			}
			break;
		case STATE_ED:
			switch (tokch) {
			case 'i': take(TOKEN_EDI);break;
			case 'x': take(TOKEN_EDX);break;
			default: tokptr--;tokimm8 = GetMax8(tokimm);take(TOKEN_IMM8);break;
			}
			break;
		case STATE_ES:
			switch (tokch) {
			case 'i': take(TOKEN_ESI);break;
			case 'p': take(TOKEN_ESP);break;
			default: tokptr--;take(TOKEN_ES);break;
			}
			break;
		case STATE_FA:
			switch (tokch) {
			case '0': tokimm = (tokimm << 4) | 0x0;toklen = 3;state = STATE_NUM3;break;
			case '1': tokimm = (tokimm << 4) | 0x1;toklen = 3;state = STATE_NUM3;break;
			case '2': tokimm = (tokimm << 4) | 0x2;toklen = 3;state = STATE_NUM3;break;
			case '3': tokimm = (tokimm << 4) | 0x3;toklen = 3;state = STATE_NUM3;break;
			case '4': tokimm = (tokimm << 4) | 0x4;toklen = 3;state = STATE_NUM3;break;
			case '5': tokimm = (tokimm << 4) | 0x5;toklen = 3;state = STATE_NUM3;break;
			case '6': tokimm = (tokimm << 4) | 0x6;toklen = 3;state = STATE_NUM3;break;
			case '7': tokimm = (tokimm << 4) | 0x7;toklen = 3;state = STATE_NUM3;break;
			case '8': tokimm = (tokimm << 4) | 0x8;toklen = 3;state = STATE_NUM3;break;
			case '9': tokimm = (tokimm << 4) | 0x9;toklen = 3;state = STATE_NUM3;break;
			case 'a': tokimm = (tokimm << 4) | 0xa;toklen = 3;state = STATE_NUM3;break;
			case 'b': tokimm = (tokimm << 4) | 0xb;toklen = 3;state = STATE_NUM3;break;
			case 'c': tokimm = (tokimm << 4) | 0xc;toklen = 3;state = STATE_NUM3;break;
			case 'd': tokimm = (tokimm << 4) | 0xd;toklen = 3;state = STATE_NUM3;break;
			case 'e': tokimm = (tokimm << 4) | 0xe;toklen = 3;state = STATE_NUM3;break;
			case 'f': tokimm = (tokimm << 4) | 0xf;toklen = 3;state = STATE_NUM3;break;
			case 'r': take(TOKEN_FAR);break;
			default: tokptr--;tokimm8 = GetMax8(tokimm);take(TOKEN_IMM8);break;
			}
			break;
		case STATE_NE:
			switch (tokch) {
			case 'a': state = STATE_NEA;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_PT:
			switch (tokch) {
			case 'r': take(TOKEN_PTR);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_SH:
			switch (tokch) {
			case 'o': state = STATE_SHO;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_TR:
			switch (tokch) {
			case '6': take(TOKEN_TR6);break;
			case '7': take(TOKEN_TR7);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_WO:
			switch (tokch) {
			case 'r': state = STATE_WOR;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_BYT:
			switch (tokch) {
			case 'e': take(TOKEN_BYTE);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_DWO:
			switch (tokch) {
			case 'r': state = STATE_DWOR;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_NEA:
			switch (tokch) {
			case 'r': take(TOKEN_NEAR);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_SHO:
			switch (tokch) {
			case 'r': state = STATE_SHOR;break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_WOR:
			switch (tokch) {
			case 'd': take(TOKEN_WORD);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_DWOR:
			switch (tokch) {
			case 'd': take(TOKEN_DWORD);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		case STATE_SHOR:
			switch (tokch) {
			case 't': take(TOKEN_SHORT);break;
			default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
			}
			break;
		default: tokptr--;flagerror = 1;take(TOKEN_NULL);break;
		}
		tokptr++;
	} while (!flagend);
	return token;
}
static void printtoken(t_aasm_token token)
{
	switch (token) {
	case TOKEN_NULL:    vapiPrint(" NULL ");break;
	case TOKEN_END:     vapiPrint(" END ");break;
	case TOKEN_LSPAREN: vapiPrint(" .[ ");break;
	case TOKEN_RSPAREN: vapiPrint(" ]. ");break;
	case TOKEN_COLON:   vapiPrint(" .: ");break;
	case TOKEN_PLUS:    vapiPrint(" .+ ");break;
	case TOKEN_MINUS:   vapiPrint(" .- ");break;
	case TOKEN_BYTE:    vapiPrint(" BYTE ");break;
	case TOKEN_WORD:    vapiPrint(" WORD ");break;
	case TOKEN_PTR:     vapiPrint(" PTR ");break;
	case TOKEN_NEAR:    vapiPrint(" NEAR ");break;
	case TOKEN_FAR:     vapiPrint(" FAR ");break;
	case TOKEN_SHORT:   vapiPrint(" SHORT ");break;
	case TOKEN_IMM8:    vapiPrint(" I8(%02X) ", tokimm8);break;
	case TOKEN_IMM16:   vapiPrint(" I16(%04X) ", tokimm16);break;
	case TOKEN_AH: vapiPrint(" AH ");break;
	case TOKEN_BH: vapiPrint(" BH ");break;
	case TOKEN_CH: vapiPrint(" CH ");break;
	case TOKEN_DH: vapiPrint(" DH ");break;
	case TOKEN_AL: vapiPrint(" AL ");break;
	case TOKEN_BL: vapiPrint(" BL ");break;
	case TOKEN_CL: vapiPrint(" CL ");break;
	case TOKEN_DL: vapiPrint(" DL ");break;
	case TOKEN_AX: vapiPrint(" AX ");break;
	case TOKEN_BX: vapiPrint(" BX ");break;
	case TOKEN_CX: vapiPrint(" CX ");break;
	case TOKEN_DX: vapiPrint(" DX ");break;
	case TOKEN_SP: vapiPrint(" SP ");break;
	case TOKEN_BP: vapiPrint(" BP ");break;
	case TOKEN_SI: vapiPrint(" SI ");break;
	case TOKEN_DI: vapiPrint(" DI ");break;
	case TOKEN_CS: vapiPrint(" CS ");break;
	case TOKEN_DS: vapiPrint(" DS ");break;
	case TOKEN_ES: vapiPrint(" ES ");break;
	case TOKEN_SS: vapiPrint(" SS ");break;
	default: vapiPrint(" ERROR ");break;
		break;
	}
}
static void matchtoken(t_aasm_token token)
{
	if (gettoken(NULL) != token) flagerror = 1;
}

/* assembly compiler: parser / grammar */
static t_aasm_oprinfo parsearg_mem(t_aasm_token token)
{
	t_aasm_oprinfo info;
	t_bool oldtoken;
	t_bool bx,bp,si,di,neg;
	t_bool eax,ecx,edx,ebx,esp,ebp,esi,edi;
	t_bool ieax,iecx,iedx,iebx,iebp,iesi,iedi;
	memset(&info, 0x00, sizeof(t_aasm_oprinfo));
	bx = bp = si = di = neg = 0;
	eax = ecx = edx = ebx = esp = ebp = esi = edi = 0;
	ieax = iecx = iedx = iebx = iebp = iesi = iedi = 0;
	info.type = TYPE_M;
	info.mod = MOD_M;
	info.sib.base = R32_EBP; //EBP for NULL Base
	info.sib.index = R32_ESP; //ESP for NULL Scale
	info.sib.scale = 0;
	oldtoken = token;
	token = gettoken(NULL);
	if (token == TOKEN_COLON) {
		switch (oldtoken) {
		case TOKEN_ES: info.flages = 1;break;
		case TOKEN_CS: info.flagcs = 1;break;
		case TOKEN_SS: info.flagss = 1;break;
		case TOKEN_DS: info.flagds = 1;break;
		case TOKEN_FS: info.flagfs = 1;break;
		case TOKEN_GS: info.flaggs = 1;break;
		default: flagerror = 1;break;
		}
	} else if (token == TOKEN_NULL || token == TOKEN_END) {
		switch (oldtoken) {
		case TOKEN_ES: 
			info.type = TYPE_SREG;
			info.mod =  MOD_R;
			info.sreg =  SREG_ES;
			break;
		case TOKEN_CS: 
			info.type = TYPE_SREG;
			info.mod =  MOD_R;
			info.sreg =  SREG_CS;
			break;
		case TOKEN_SS: 
			info.type = TYPE_SREG;
			info.mod =  MOD_R;
			info.sreg =  SREG_SS;
			break;
		case TOKEN_DS: 
			info.type = TYPE_SREG;
			info.mod =  MOD_R;
			info.sreg =  SREG_DS;
			break;
		case TOKEN_FS: 
			info.type = TYPE_SREG;
			info.mod =  MOD_R;
			info.sreg =  SREG_FS;
			break;
		case TOKEN_GS: 
			info.type = TYPE_SREG;
			info.mod =  MOD_R;
			info.sreg =  SREG_GS;
			break;
		default: flagerror = 1;break;
		}
		return info;
	} else flagerror = 1;
	matchtoken(TOKEN_LSPAREN);
	token = gettoken(NULL);
	while (token != TOKEN_RSPAREN && !flagerror) {
		switch (token) {
		case TOKEN_PLUS:break;
		case TOKEN_MINUS:
			token = gettoken(NULL);
			neg = 1;
			switch (token) {
			case TOKEN_IMM8:
				if (info.mod != MOD_M) flagerror = 1;
				if (tokimm8 > 0x80) flagerror = 1;
				else {
					tokimm8 = (~tokimm8) + 1;
					info.disp8 = tokimm8;
					info.mod = MOD_M_DISP8;
				}
				break;
			case TOKEN_IMM16:
				if (info.mod != MOD_M) flagerror = 1;
				if (tokimm16 > 0xff80) flagerror = 1;
				else {
					tokimm16 = (~tokimm16) + 1;
					info.disp16 = tokimm16;
					info.mod = MOD_M_DISP16;
				}
				break;
			case TOKEN_IMM32:
				if (info.mod != MOD_M) flagerror = 1;
				if (tokimm32 > 0xffffff80) flagerror = 1;
				else {
					tokimm32 = (~tokimm32) + 1;
					info.disp32 = tokimm32;
					info.mod = MOD_M_DISP32;
				}
				break;
			default: flagerror = 1;break;
			}
			break;
		case TOKEN_BX: if (bx) flagerror = 1; else bx = 1;break;
		case TOKEN_SI: if (si) flagerror = 1; else si = 1;break;
		case TOKEN_BP: if (bp) flagerror = 1; else bp = 1;break;
		case TOKEN_DI: if (di) flagerror = 1; else di = 1;break;
		case TOKEN_IMM8:
			if (info.mod != MOD_M) flagerror = 1;
			info.mod = MOD_M_DISP8;
			info.disp8 = tokimm8;
			break;
		case TOKEN_IMM16:
			if (info.mod != MOD_M) flagerror = 1;
			info.mod = MOD_M_DISP16;
			info.disp16 = tokimm16;
			break;
		case TOKEN_IMM32:
			if (info.mod != MOD_M) flagerror = 1;
			info.mod = MOD_M_DISP32;
			info.disp32 = tokimm32;
			break;
		case TOKEN_EAX:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (ieax) flagerror = 1;
				else {
					ieax = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_EAX;
					}
				}
			} else {
				if (eax) flagerror = 1;
				else {
					eax = 1;
					info.sib.base = R32_EAX;
				}
			}
			continue;
			break;
		case TOKEN_ECX:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (iecx) flagerror = 1;
				else {
					iecx = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_ECX;
					}
				}
			} else {
				if (ecx) flagerror = 1;
				else {
					ecx = 1;
					info.sib.base = R32_ECX;
				}
			}
			continue;
			break;
		case TOKEN_EDX:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (iedx) flagerror = 1;
				else {
					iedx = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_EDX;
					}
				}
			} else {
				if (edx) flagerror = 1;
				else {
					edx = 1;
					info.sib.base = R32_EDX;
				}
			}
			continue;
			break;
		case TOKEN_EBX:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (iebx) flagerror = 1;
				else {
					iebx = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_EBX;
					}
				}
			} else {
				if (ebx) flagerror = 1;
				else {
					ebx = 1;
					info.sib.base = R32_EBX;
				}
			}
			continue;
			break;
		case TOKEN_ESP:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) flagerror = 1;
			else {
				if (esp) flagerror = 1;
				else {
					esp = 1;
					info.sib.base = R32_ESP;
				}
			}
			continue;
			break;
		case TOKEN_EBP:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (iebp) flagerror = 1;
				else {
					iebx = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_EBP;
					}
				}
			} else {
				if (ebp) flagerror = 1;
				else {
					ebp = 1;
					info.sib.base = R32_EBP;
				}
			}
			continue;
			break;
		case TOKEN_ESI:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (iesi) flagerror = 1;
				else {
					iesi = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_ESI;
					}
				}
			} else {
				if (esi) flagerror = 1;
				else {
					esi = 1;
					info.sib.base = R32_ESI;
				}
			}
			continue;
			break;
		case TOKEN_EDI:
			token = gettoken(NULL);
			if (token == TOKEN_TIMES) {
				if (iedi) flagerror = 1;
				else {
					iedi = 1;
					token = gettoken(NULL);
					if (token != TOKEN_IMM8) flagerror = 1;
					else {
						info.sib.scale = tokimm8;
						info.sib.index = R32_EDI;
					}
				}
			} else {
				if (edi) flagerror = 1;
				else {
					edi = 1;
					info.sib.base = R32_EDI;
				}
			}
			continue;
			break;
		default: flagerror = 1;break;}
		token = gettoken(NULL);
	}
	token = gettoken(NULL);
	if (flagerror || token != TOKEN_END) {
		info.type = TYPE_NONE;
		printtoken(token);
		return info;
	}

	if (bx || bp || si || di || info.mod == MOD_M_DISP16) {
		if (!bx && !si && !bp && !di) {
			info.mem = MEM_BP;
			if (info.mod == MOD_M_DISP16)
				info.mod = MOD_M;
			else flagerror = 1;
		} else {
			if ( bx &&  si && !bp && !di) info.mem = MEM_BX_SI;
			else if ( bx && !si && !bp &&  di) info.mem = MEM_BX_DI;
			else if (!bx &&  si &&  bp && !di) info.mem = MEM_BP_SI;
			else if (!bx && !si &&  bp &&  di) info.mem = MEM_BP_DI;
			else if ( bx && !si && !bp && !di) info.mem = MEM_BX;
			else if (!bx &&  si && !bp && !di) info.mem = MEM_SI;
			else if (!bx && !si &&  bp && !di) {
				info.mem = MEM_BP;
				if (info.mod == MOD_M) {
					info.mod = MOD_M_DISP8;
					info.disp8 = 0x00;
				}
			} else if (!bx && !si && !bp && di) info.mem = MEM_DI;
			else flagerror = 1;
			if (info.mod == MOD_M_DISP16) {
				if (info.disp16 < 0x0080 || info.disp16 > 0xff7f) flagerror = 1;
			}
		}
	} else if (eax || ecx || edx || ebx || esp || ebp || esi || edi ||
		ieax || iecx || iedx || iebx || iebp || iesi || iedi || info.mod == MOD_M_DISP32) {
		if (!eax && !ecx && !edx && !ebx && !esp && !ebp && !esi && !edi &&
			!ieax && !iecx && !iedx && !iebx && !iebp && !iesi && !iedi) {
			if (info.mod == MOD_M_DISP32) {
				info.mem = MEM_EBP;
				info.mod = MOD_M;
			} else flagerror = 1;
		} else {
			if (esp || ieax || iecx || iedx || iebx || iebp || iesi || iedi) {
				info.mem = MEM_SIB;
			} else if (eax) info.mem = MEM_EAX;
			else if (ecx) info.mem = MEM_ECX;
			else if (edx) info.mem = MEM_EDX;
			else if (ebx) info.mem = MEM_EBX;
			else if (ebp) info.mem = MEM_EBP;
			else if (esi) info.mem = MEM_ESI;
			else if (edi) info.mem = MEM_EDI;
			else flagerror = 1;
			if (info.mod == MOD_M_DISP32) {
				if (info.disp32 < 0x00000080 || info.disp32 > 0xffffff7f) flagerror = 1;
			}
		}
	} else flagerror = 1;
	switch (info.mem) {
	case MEM_BX_SI:
	case MEM_BX_DI:
	case MEM_BX:
	case MEM_SI:
	case MEM_DI:
		if (info.flagds) info.flagds = 0;
		break;
	case MEM_BP_SI:
	case MEM_BP_DI:
		if (info.flagss) info.flagss = 0;
		break;
	case MEM_BP:
		if (!bp && info.flagds) info.flagds = 0;
		else if (bp && info.flagss) info.flagss = 0;
		break;
	case MEM_EAX:
	case MEM_ECX:
	case MEM_EDX:
	case MEM_EBX:
	case MEM_ESI:
	case MEM_EDI:
		if (info.flagds) info.flagds = 0;
		break;
	case MEM_EBP:
		if (info.flagss) info.flagss = 0;
		break;
	case MEM_SIB:
		if (info.sib.base == R32_ESP ||
			(info.sib.base == R32_EBP && info.mod != MOD_M)) {
			if (info.flagss) info.flagss = 0;
		} else if (info.flagds) info.flagds = 0;		
		break;
	}
	info.type = TYPE_M;
	return info;
}
static t_aasm_oprinfo parsearg_imm(t_aasm_token token)
{
	t_aasm_oprinfo info;
	memset(&info, 0x00, sizeof(t_aasm_oprinfo));
	info.type = TYPE_NONE;
	info.mod = MOD_M;
	info.imms = 0;
	info.immn = 0;
	info.ptr = PTR_NONE;
	if (token == TOKEN_PLUS) {
		info.imms = 1;
		info.immn = 0;
		token = gettoken(NULL);
	} else if (token == TOKEN_MINUS) {
		info.imms = 1;
		info.immn = 1;
		token = gettoken(NULL);
	}
	if (token == TOKEN_IMM8) {
		info.type = TYPE_I8;
		if (!info.immn) info.imm8 = tokimm8;
		else info.imm8 = (~tokimm8) + 1;
	} else if (token == TOKEN_IMM16) {
		info.type = TYPE_I16;
		if (!info.immn) info.imm16 = tokimm16;
		else info.imm16 = (~tokimm16) + 1;
	} else if (token == TOKEN_IMM32) {
		info.type = TYPE_I32;
		if (!info.immn) info.imm32 = tokimm32;
		else info.imm32 = (~tokimm32) + 1;
	} else flagerror = 1;

	token = gettoken(NULL);
	if (!info.imms && token == TOKEN_COLON) {
		if (info.type == TYPE_I16) info.pcs = info.imm16;
		else flagerror = 1;
		token = gettoken(NULL);
		if (token == TOKEN_IMM16) info.pip = tokimm16;
		else flagerror = 1;
		info.type = TYPE_I16_16;
	}

	return info;
}
static t_aasm_oprinfo parsearg(t_strptr arg)
{
	t_aasm_token token;
	t_aasm_oprinfo info;
//	vapiPrint("parsearg: \"%s\"\n", arg);
	memset(&info, 0x00 ,sizeof(t_aasm_oprinfo));
	if (!arg || !arg[0]) {
		info.type = TYPE_NONE;
		return info;
	}
	token = gettoken(arg);
	switch (token) {
	case TOKEN_LABEL:
		info.type = TYPE_LABEL;
		info.ptr = PTR_NONE;
		STRCPY(info.label, toklabel);
		break;
	case TOKEN_CHAR:
	case TOKEN_STRING:
	case TOKEN_NULL:
	case TOKEN_END:
		info.type = TYPE_NONE;
		break;
	case TOKEN_BYTE:
		token = gettoken(NULL);
		if (token == TOKEN_PTR) token = gettoken(NULL);
		info = parsearg_mem(token);
		info.type = TYPE_M8;
		break;
	case TOKEN_WORD:
		token = gettoken(NULL);
		if (token == TOKEN_PTR) token = gettoken(NULL);
		info = parsearg_mem(token);
		info.type = TYPE_M16;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_DWORD:
		token = gettoken(NULL);
		if (token == TOKEN_PTR) token = gettoken(NULL);
		info = parsearg_mem(token);
		info.type = TYPE_M32;
		info.ptr = PTR_FAR;
		break;
	case TOKEN_AL:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_AL;
		break;
	case TOKEN_CL:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_CL;
		break;
	case TOKEN_DL:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_DL;
		break;
	case TOKEN_BL:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_BL;
		break;
	case TOKEN_AH:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_AH;
		break;
	case TOKEN_CH:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_CH;
		break;
	case TOKEN_DH:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_DH;
		break;
	case TOKEN_BH:
		info.type = TYPE_R8;
		info.mod = MOD_R;
		info.reg8 = R8_BH;
		break;
	case TOKEN_AX:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_AX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_CX:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_CX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_DX:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_DX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_BX:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_BX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_SP:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_SP;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_BP:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_BP;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_SI:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_SI;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_DI:
		info.type = TYPE_R16;
		info.mod = MOD_R;
		info.reg16 = R16_DI;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_EAX:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_EAX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_ECX:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_ECX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_EDX:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_EDX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_EBX:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_EBX;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_ESP:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_ESP;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_EBP:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_EBP;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_ESI:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_ESI;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_EDI:
		info.type = TYPE_R32;
		info.mod = MOD_R;
		info.reg32 = R32_EDI;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_ES:
	case TOKEN_CS:
	case TOKEN_SS:
	case TOKEN_DS:
	case TOKEN_FS:
	case TOKEN_GS:
		info = parsearg_mem(token);
		break;
	case TOKEN_PLUS:
	case TOKEN_MINUS:
	case TOKEN_IMM8:
	case TOKEN_IMM16:
	case TOKEN_IMM32:
		info = parsearg_imm(token);
		if (info.type == TYPE_I16_16)
			info.ptr = PTR_FAR;
		else
			info.ptr = PTR_NONE;
		break;
	case TOKEN_SHORT:
		token = gettoken(NULL);
		if (token == TOKEN_PTR) token = gettoken(NULL);
		if (token == TOKEN_PLUS || token == TOKEN_MINUS) {
			info = parsearg_imm(token);
			if (info.type != TYPE_I8) flagerror = 1;
		} else if (token == TOKEN_LABEL) {
			info.type = TYPE_LABEL;
			STRCPY(info.label, toklabel);
		} else flagerror = 1;
		info.ptr = PTR_SHORT;
		break;
	case TOKEN_NEAR:
		token = gettoken(NULL);
		if (token == TOKEN_PTR) token = gettoken(NULL);
		if (token == TOKEN_WORD) {
			token = gettoken(NULL);
			if (token == TOKEN_PTR) token = gettoken(NULL);
			if (token != TOKEN_LSPAREN) flagerror = 1;
			info = parsearg_mem(token);
			info.type = TYPE_M16;
		} else if (token == TOKEN_IMM8 || token == TOKEN_IMM16) {
			info = parsearg_imm(token);
			if (info.type == TYPE_I8) {
				info.imm16 = (t_nubit8)info.imm8;
				info.type = TYPE_I16;
			}
			if (info.type != TYPE_I16) flagerror = 1;
		} else if (token == TOKEN_LABEL) {
			info.type = TYPE_LABEL;
			STRCPY(info.label, toklabel);
		} else flagerror = 1;
		info.ptr = PTR_NEAR;
		break;
	case TOKEN_FAR:
		token = gettoken(NULL);
		if (token == TOKEN_PTR) token = gettoken(NULL);
		switch (token) {
		case TOKEN_WORD:
			token = gettoken(NULL);
			if (token == TOKEN_PTR) token = gettoken(NULL);
			info = parsearg_mem(token);
			info.type = TYPE_M16;
			info.ptr = PTR_FAR;
			break;
		case TOKEN_DWORD:
			token = gettoken(NULL);
			if (token == TOKEN_PTR) token = gettoken(NULL);
			info = parsearg_mem(token);
			info.type = TYPE_M32;
			info.ptr = PTR_FAR;
			break;
		case TOKEN_IMM16:
			info = parsearg_imm(token);
			if (info.type != TYPE_I16_16) flagerror = 1;
			break;
		case TOKEN_LABEL:
			info.type = TYPE_LABEL;
			STRCPY(info.label, toklabel);
		case TOKEN_ES:
		case TOKEN_CS:
		case TOKEN_SS:
		case TOKEN_DS:
		case TOKEN_FS:
		case TOKEN_GS:
			info = parsearg_mem(token);
			if (info.type != TYPE_M) flagerror = 1;
			break;
		default:flagerror = 1;break;
		}
		info.ptr = PTR_FAR;
		break;
	case TOKEN_CR0: info.type = TYPE_CREG;info.creg = CREG_CR0;break;
	case TOKEN_CR2: info.type = TYPE_CREG;info.creg = CREG_CR2;break;
	case TOKEN_CR3: info.type = TYPE_CREG;info.creg = CREG_CR3;break;
	case TOKEN_DR0: info.type = TYPE_DREG;info.dreg = DREG_DR0;break;
	case TOKEN_DR1: info.type = TYPE_DREG;info.dreg = DREG_DR1;break;
	case TOKEN_DR2: info.type = TYPE_DREG;info.dreg = DREG_DR2;break;
	case TOKEN_DR3: info.type = TYPE_DREG;info.dreg = DREG_DR3;break;
	case TOKEN_DR6: info.type = TYPE_DREG;info.dreg = DREG_DR6;break;
	case TOKEN_DR7: info.type = TYPE_DREG;info.dreg = DREG_DR7;break;
	case TOKEN_TR6: info.type = TYPE_TREG;info.treg = TREG_TR6;break;
	case TOKEN_TR7: info.type = TYPE_TREG;info.treg = TREG_TR7;break;
	default:
		info.type = TYPE_NONE;
		flagerror = 1;
		break;
	}
	return info;
}
/* assembly compiler: analyzer / label table */
typedef struct tag_t_aasm_label_ref_node {
	t_aasm_oprptr ptr;
	struct tag_t_aasm_label_ref_node *next;
	t_nubit16 cs,ip;
} t_aasm_label_ref_node;

typedef struct tag_t_aasm_label_def_node {
	char name[0x100];
	struct tag_t_aasm_label_ref_node *ref;
	struct tag_t_aasm_label_def_node *next;
	t_nubit16 cs,ip;
} t_aasm_label_def_node;

static t_aasm_label_def_node *label_entry = NULL;

static t_aasm_label_def_node *labelNewDefNode(t_strptr name, t_nubit16 pcs, t_nubit16 pip)
{
	t_aasm_label_def_node *p = (t_aasm_label_def_node *)malloc(sizeof(t_aasm_label_def_node));
	STRCPY(p->name, name);
	p->cs = pcs;
	p->ip = pip;
	p->next = NULL;
	p->ref = NULL;
	return p;
}
static t_aasm_label_ref_node *labelNewRefNode(t_aasm_oprptr pptr, t_nubit16 pcs, t_nubit16 pip)
{
	t_aasm_label_ref_node *p = (t_aasm_label_ref_node *)malloc(sizeof(t_aasm_label_ref_node));
	p->ptr = pptr;
	p->cs = pcs;
	p->ip = pip;
	p->next = NULL;
	return p;
}

static void labelRealizeRef(t_aasm_label_def_node *pdef, t_aasm_label_ref_node *pref)
{
	t_nubit16 lo, hi, ta;
	t_nsbit8 rel8;
	if (!pdef || !pref) flagerror = 1;
	if (flagerror) return;
	//printf("realize: target %04X:%04X current %04X:%04X\n",pdef->cs,pdef->ip,pref->cs,pref->ip);
	//printf("ptr: %d, name: %s\n",pref->ptr,pdef->name);
	switch (pref->ptr) {
	case PTR_FAR:
		vramRealWord(pref->cs, pref->ip + 0) = pdef->ip;
		vramRealWord(pref->cs, pref->ip + 2) = pdef->cs;
		break;
	case PTR_NEAR:
		vramRealWord(pref->cs, pref->ip + 0) = pdef->ip - pref->ip - 0x02;
		break;
	case PTR_SHORT:
		lo = pref->ip - 0x0080 + 0x0001;
		hi = pref->ip + 0x007f + 0x0001;
		ta = pdef->ip;
		if (pref->ip < lo || pref->ip > hi)
			if (ta <= hi || ta >= lo)
				rel8 = ta - pref->ip - 0x0001;
			else flagerror = 1;
		else if (ta <= hi && ta >= lo)
			rel8 = ta - pref->ip - 0x0001;
		else flagerror = 1;
		//printf("lo: %x, hi: %x, ta = %x, rel8 = %x\n",lo, hi, ta, rel8 & 0xff);
		if (flagerror) return;
		vramRealByte(pref->cs, pref->ip + 0) = rel8;
		break;
	case PTR_NONE:
	default:
		flagerror = 1;
		break;
	}
}
static void labelRemoveRefList(t_aasm_label_def_node *pdef)
{
	t_aasm_label_ref_node *p = NULL, *q = NULL;
	if (!pdef) return;
	p = pdef->ref;
	while (p) {
		q = p->next;
		labelRealizeRef(pdef, p);
		free(p);
		p = q;
	}
	pdef->ref = NULL;
}
static void labelRemoveDefList()
{
	t_aasm_label_def_node *p = label_entry, *q = NULL;
	if (!p) return;
	while (p) {
		q = p->next;
		labelRemoveRefList(p);
		free(p);
		p = q;
	}
	label_entry = NULL;
}
static void labelRealizeDefList()
{
	t_aasm_label_def_node *p = label_entry;
	while (p) {
		labelRemoveRefList(p);
		p = p->next;
	}
}
static void labelStoreDef(t_strptr strlabel)
{
	t_bool flagfound = 0;
	t_aasm_label_def_node *p = label_entry, *q = NULL;
	while (p && !flagerror) {
		q = p;
		if (!strcmp(p->name, strlabel)) {
			if (p->cs || p->ip) labelRemoveRefList(p);
			p->cs = avcs;
			p->ip = avip;
			flagfound = 1;
			labelRemoveRefList(p);
			//printf("def replaced: '%s' at %04X:%04X\n", strlabel, avcs, avip);
			break;
		}
		p = p->next;
	}
	if (flagfound || flagerror) return;
	if (!q) label_entry = labelNewDefNode(strlabel, avcs, avip);
	else q->next = labelNewDefNode(strlabel, avcs, avip);
	//printf("def saved: '%s' at %04X:%04X\n", strlabel, avcs, avip);
}
static void labelStoreRef(t_strptr strlabel, t_aasm_oprptr ptrlabel)
{
	t_aasm_label_def_node *p = label_entry;
	t_aasm_label_ref_node *r = NULL, *s = NULL, *n = NULL;
	while (p && strcmp(p->name, strlabel) && !flagerror)
		p = p->next;
	if (flagerror) return;
	n = labelNewRefNode(ptrlabel, avcs, avip);
	if (!p) {
		labelStoreDef(strlabel);
		p = label_entry;
		while (p && strcmp(p->name, strlabel) && !flagerror)
			p = p->next;
		p->cs = p->ip = 0x0000;
	} else if (p && (p->cs || p->ip)) {
		//printf("ref real: '%s' at %04X:%04X\n", strlabel, avcs, avip);
		labelRealizeRef(p, n);
		//printf("result: %04X:%04X\n",vramRealWord(avcs, avip+2),vramRealWord(avcs, avip));
		free(n);
		return;
	}
	r = p->ref;
	while (r) {
		s = r;
		r = r->next;
	}
	if (s) s->next = n;
	else p->ref = n;	
	//printf("ref saved: '%s' at %04X:%04X\n", strlabel, avcs, avip);
}

static void setbyte(t_nubit8 byte)
{
	d_nubit8(acode + iop) = byte;
	iop += 1;
}
static void setword(t_nubit16 word)
{
	d_nubit16(acode + iop) = word;
	iop += 2;
}
static void setdword(t_nubit32 dword)
{
	d_nubit32(acode + iop) = dword;
	iop += 4;
}
static void LABEL()
{
	t_aasm_token token;
	token = gettoken(aop);
	setbyte(0x90);
	avip++;
	if (token == TOKEN_LABEL) labelStoreDef(toklabel); 
	matchtoken(TOKEN_COLON);
}

static void _c_imm8(t_nubit8 byte)
{
	setbyte(byte);
}
static void _c_imm16(t_nubit16 word)
{
	setword(word);
}
static void _c_imm32(t_nubit32 dword)
{
	setdword(dword);
}
static void _c_modrm(t_aasm_oprinfo modrm, t_nubit8 reg)
{
	t_nubit8 sibval;
	t_nubit8 modregrm = (reg << 3);
	switch (modrm.mem) {
	case MEM_BX_SI:
	case MEM_BX_DI:
	case MEM_BP_SI:
	case MEM_BP_DI:
	case MEM_SI:
	case MEM_DI:
	case MEM_BP:
	case MEM_BX:
		_SetAddressSize(2);
		switch(modrm.mod) {
		case MOD_M:
			modregrm |= (0 << 6);
			modregrm |= (t_nubit8)modrm.mem;
			setbyte(modregrm);
			switch(modrm.mem) {
			case MEM_BP: setword(modrm.disp16);break;
			default:break;}
			break;
		case MOD_M_DISP8:
			modregrm |= (1 << 6);
			modregrm |= (t_nubit8)modrm.mem;
			setbyte(modregrm);
			setbyte(modrm.disp8);
			break;
		case MOD_M_DISP16:
			modregrm |= (2 << 6);
			modregrm |= (t_nubit8)modrm.mem;
			setbyte(modregrm);
			setword(modrm.disp16);
			break;
		case MOD_R:
			modregrm |= (3 << 6);
			switch (modrm.type) {
			case TYPE_R8:
				modregrm |= (t_nubit8)modrm.reg8;
				setbyte(modregrm);
				break;
			case TYPE_R16:
				modregrm |= (t_nubit8)modrm.reg16;
				setbyte(modregrm);
				break;
			case TYPE_R32:
				modregrm |= (t_nubit8)modrm.reg32;
				setbyte(modregrm);
				break;
			default:flagerror = 1;break;}
			break;
		default:flagerror = 1;break;}
		break;
	case MEM_EAX:
	case MEM_ECX:
	case MEM_EDX:
	case MEM_EBX:
	case MEM_SIB:
	case MEM_EBP:
	case MEM_ESI:
	case MEM_EDI:
		_SetAddressSize(4);
		switch(modrm.mod) {
		case MOD_M:
			modregrm |= (0 << 6);
			modregrm |= (t_nubit8)modrm.mem & 0x07;
			setbyte(modregrm);
			switch(modrm.mem) {
			case MEM_SIB:
				sibval = (t_nubit8)modrm.sib.base;
				sibval |= ((t_nubit8)modrm.sib.index << 3);
				switch (modrm.sib.scale) {
				case 0: modrm.sib.scale = 0;break;
				case 1: modrm.sib.scale = 0;break;
				case 2: modrm.sib.scale = 1;break;
				case 4: modrm.sib.scale = 2;break;
				case 8: modrm.sib.scale = 3;break;
				default:flagerror = 1;break;}
				sibval |= (modrm.sib.scale << 3);
				setbyte(sibval);
				break;
			case MEM_EBP: setdword(modrm.disp32);break;
			default:break;}
			break;
		case MOD_M_DISP8:
			modregrm |= (1 << 6);
			modregrm |= (t_nubit8)modrm.mem & 0x07;
			setbyte(modregrm);
			switch(modrm.mem) {
			case MEM_SIB:
				sibval = (t_nubit8)modrm.sib.base;
				sibval |= ((t_nubit8)modrm.sib.index << 3);
				switch (modrm.sib.scale) {
				case 0: modrm.sib.scale = 0;break;
				case 1: modrm.sib.scale = 0;break;
				case 2: modrm.sib.scale = 1;break;
				case 4: modrm.sib.scale = 2;break;
				case 8: modrm.sib.scale = 3;break;
				default:flagerror = 1;break;}
				sibval |= (modrm.sib.scale << 3);
				setbyte(sibval);
				break;
			default:break;}
			setbyte(modrm.disp8);
			break;
		case MOD_M_DISP32:
			modregrm |= (2 << 6);
			modregrm |= (t_nubit8)modrm.mem & 0x07;
			setbyte(modregrm);
			switch(modrm.mem) {
			case MEM_SIB:
				sibval = (t_nubit8)modrm.sib.base;
				sibval |= ((t_nubit8)modrm.sib.index << 3);
				switch (modrm.sib.scale) {
				case 0: modrm.sib.scale = 0;break;
				case 1: modrm.sib.scale = 0;break;
				case 2: modrm.sib.scale = 1;break;
				case 4: modrm.sib.scale = 2;break;
				case 8: modrm.sib.scale = 3;break;
				default:flagerror = 1;break;}
				sibval |= (modrm.sib.scale << 3);
				setbyte(sibval);
				break;
			default:break;}
			setword(modrm.disp32);
			break;
		case MOD_R:
			modregrm |= (3 << 6);
			switch (modrm.type) {
			case TYPE_R8:
				modregrm |= (t_nubit8)modrm.reg8;
				setbyte(modregrm);
				break;
			case TYPE_R16:
				modregrm |= (t_nubit8)modrm.reg16;
				setbyte(modregrm);
				break;
			case TYPE_R32:
				modregrm |= (t_nubit8)modrm.reg32;
				setbyte(modregrm);
				break;
			default:flagerror = 1;break;}
			break;
		default:flagerror = 1;break;}
		break;
	default: flagerror = 1;break;}
}

static void ADD_RM8_R8()
{
	setbyte(0x00);
	_c_modrm(aopri1, aopri2.reg8);
}
static void ADD_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x01);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void ADD_R8_RM8()
{
	setbyte(0x02);
	_c_modrm(aopri2, aopri1.reg8);
}
static void ADD_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x03);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void ADD_AL_I8()
{
	setbyte(0x04);
	_c_imm8(aopri2.imm8);
}
static void ADD_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x05);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void PUSH_ES()
{
	setbyte(0x06);
	avip++;
}
static void POP_ES()
{
	setbyte(0x07);
	avip++;
}
static void OR_RM8_R8()
{
	setbyte(0x08);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void OR_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x09);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void OR_R8_RM8()
{
	setbyte(0x0a);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void OR_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x0b);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void OR_AL_I8()
{
	setbyte(0x0c);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void OR_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x0d);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void PUSH_CS()
{
	setbyte(0x0e);
	avip++;
}
static void POP_CS()
{
	setbyte(0x0f);
	avip++;
}
static void ADC_RM8_R8()
{
	setbyte(0x10);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void ADC_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x11);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void ADC_R8_RM8()
{
	setbyte(0x12);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void ADC_R16_RM16()
{
	setbyte(0x13);
	avip++;
	_c_modrm(aopri2, aopri1.reg16);
}
static void ADC_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x13);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void ADC_AL_I8()
{
	setbyte(0x14);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void ADC_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x15);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void PUSH_SS()
{
	setbyte(0x16);
	avip++;
}
static void POP_SS()
{
	setbyte(0x17);
	avip++;
}
static void SBB_RM8_R8()
{
	setbyte(0x18);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void SBB_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x19);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void SBB_R8_RM8()
{
	setbyte(0x1a);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void SBB_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x1b);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void SBB_AL_I8()
{
	setbyte(0x1c);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void SBB_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x1d);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void PUSH_DS()
{
	setbyte(0x1e);
	avip++;
}
static void POP_DS()
{
	setbyte(0x1f);
	avip++;
}
static void AND_RM8_R8()
{
	setbyte(0x20);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void AND_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x21);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void AND_R8_RM8()
{
	setbyte(0x22);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void AND_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x23);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void AND_AL_I8()
{
	setbyte(0x24);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void AND_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x25);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void ES()
{
	if (ARG_NONE) {
		setbyte(0x26);
		avip++;
	} else flagerror = 1;
}
static void DAA()
{
	if (ARG_NONE) {
		setbyte(0x27);
		avip++;
	} else flagerror = 1;
}
static void SUB_RM8_R8()
{
	setbyte(0x28);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void SUB_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x29);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void SUB_R8_RM8()
{
	setbyte(0x2a);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void SUB_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x2b);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void SUB_AL_I8()
{
	setbyte(0x2c);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void SUB_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x2d);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void CS()
{
	if (ARG_NONE) {
		setbyte(0x2e);
		avip++;
	} else flagerror = 1;
}
static void DAS()
{
	if (ARG_NONE) {
		setbyte(0x2f);
		avip++;
	} else flagerror = 1;
}
static void XOR_RM8_R8()
{
	setbyte(0x30);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void XOR_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x31);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void XOR_R8_RM8()
{
	setbyte(0x32);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void XOR_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x33);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void XOR_AL_I8()
{
	setbyte(0x34);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void XOR_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x35);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void SS()
{
	if (ARG_NONE) {
		setbyte(0x36);
		avip++;
	} else flagerror = 1;
}
static void AAA()
{
	if (ARG_NONE) {
		setbyte(0x37);
		avip++;
	} else flagerror = 1;
}
static void CMP_RM8_R8()
{
	setbyte(0x38);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void CMP_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x39);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void CMP_R8_RM8()
{
	setbyte(0x3a);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void CMP_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x3b);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void CMP_AL_I8()
{
	setbyte(0x3c);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void CMP_EAX_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x3d);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void DS()
{
	if (ARG_NONE) {
		setbyte(0x3e);
		avip++;
	} else flagerror = 1;
}
static void AAS()
{
	if (ARG_NONE) {
		setbyte(0x3f);
		avip++;
	} else flagerror = 1;
}
static void INC_AX()
{
	setbyte(0x40);
	avip++;
}
static void INC_CX()
{
	setbyte(0x41);
	avip++;
}
static void INC_DX()
{
	setbyte(0x42);
	avip++;
}
static void INC_BX()
{
	setbyte(0x43);
	avip++;
}
static void INC_SP()
{
	setbyte(0x44);
	avip++;
}
static void INC_BP()
{
	setbyte(0x45);
	avip++;
}
static void INC_SI()
{
	setbyte(0x46);
	avip++;
}
static void INC_DI()
{
	setbyte(0x47);
	avip++;
}
static void DEC_AX()
{
	setbyte(0x48);
	avip++;
}
static void DEC_CX()
{
	setbyte(0x49);
	avip++;
}
static void DEC_DX()
{
	setbyte(0x4a);
	avip++;
}
static void DEC_BX()
{
	setbyte(0x4b);
	avip++;
}
static void DEC_SP()
{
	setbyte(0x4c);
	avip++;
}
static void DEC_BP()
{
	setbyte(0x4d);
	avip++;
}
static void DEC_SI()
{
	setbyte(0x4e);
	avip++;
}
static void DEC_DI()
{
	setbyte(0x4f);
	avip++;
}
static void PUSH_EAX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x50);
}
static void PUSH_ECX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x51);
}
static void PUSH_EDX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x52);
}
static void PUSH_EBX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x53);
}
static void PUSH_ESP(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x54);
}
static void PUSH_EBP(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x55);
}
static void PUSH_ESI(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x56);
}
static void PUSH_EDI(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x57);
}
static void POP_EAX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x58);
}
static void POP_ECX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x59);
}
static void POP_EDX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x5a);
}
static void POP_EBX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x5b);
}
static void POP_ESP(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x5c);
}
static void POP_EBP(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x5d);
}
static void POP_ESI(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x5e);
}
static void POP_EDI(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x5f);
}
static void PUSH_I32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x68);
	switch (byte) {
	case 2: _c_imm16(aopri1.imm16);break;
	case 4: _c_imm32(aopri1.imm32);break;
	default:flagerror = 1;break;}
}
static void PUSH_I8()
{
	setbyte(0x6a);
	_c_imm8(aopri1.imm8);
}
static void INS_80(t_nubit8 rid)
{
	setbyte(0x80);
	_c_modrm(aopri1, rid);
	_c_imm8(aopri2.imm8);
}
static void INS_81(t_nubit8 rid, t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x81);
	_c_modrm(aopri1, rid);
	switch (byte) {
	case 2: _c_imm16(aopri2.imm16);break;
	case 4: _c_imm32(aopri2.imm32);break;
	default:flagerror = 1;break;}
}
static void INS_83(t_nubit8 rid, t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x83);
	_c_modrm(aopri1, rid);
	_c_imm8(aopri2.imm8);
}
static void TEST_RM8_R8()
{
	setbyte(0x84);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void TEST_RM16_R16()
{
	setbyte(0x85);
	avip++;
	_c_modrm(aopri1, aopri2.reg16);
}
static void XCHG_RM8_R8()
{
	setbyte(0x86);
	_c_modrm(aopri1, aopri2.reg8);
}
static void XCHG_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x87);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void MOV_RM8_R8()
{
	setbyte(0x88);
	avip++;
	_c_modrm(aopri1, aopri2.reg8);
}
static void MOV_RM32_R32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x89);
	switch (byte) {
	case 2: _c_modrm(aopri1, aopri2.reg16);break;
	case 4: _c_modrm(aopri1, aopri2.reg32);break;
	default:flagerror = 1;break;}
}
static void MOV_R8_RM8()
{
	setbyte(0x8a);
	avip++;
	_c_modrm(aopri2, aopri1.reg8);
}
static void MOV_R32_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x8b);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void MOV_RM16_SREG()
{
	setbyte(0x8c);
	avip++;
	_c_modrm(aopri1, aopri2.sreg);
}
static void LEA_R16_M16()
{
	setbyte(0x8d);
	avip++;
	_c_modrm(aopri2, aopri1.reg16);
}
static void MOV_SREG_RM16()
{
	setbyte(0x8e);
	_c_modrm(aopri2, aopri1.sreg);
}
static void POP_RM32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x8f);
	_c_modrm(aopri1, 0x00);
}
static void NOP()
{
	if (ARG_NONE) {
		setbyte(0x90);
		avip++;
	} else flagerror = 1;
}
static void XCHG_AX_AX()
{
	setbyte(0x90);
	avip++;
}
static void XCHG_CX_AX()
{
	setbyte(0x91);
	avip++;
}
static void XCHG_DX_AX()
{
	setbyte(0x92);
	avip++;
}
static void XCHG_BX_AX()
{
	setbyte(0x93);
	avip++;
}
static void XCHG_SP_AX()
{
	setbyte(0x94);
	avip++;
}
static void XCHG_BP_AX()
{
	setbyte(0x95);
	avip++;
}
static void XCHG_SI_AX()
{
	setbyte(0x96);
	avip++;
}
static void XCHG_DI_AX()
{
	setbyte(0x97);
	avip++;
}
static void CBW()
{
	if (ARG_NONE) {
		setbyte(0x98);
		avip++;
	} else flagerror = 1;
}
static void CWD()
{
	if (ARG_NONE) {
		setbyte(0x99);
		avip++;
	} else flagerror = 1;
}
static void CALL_PTR16_16()
{
	setbyte(0x9a);
	avip++;
	if (ARG_FAR_LABEL) {
		labelStoreRef(aopri1.label, PTR_FAR);
		avip += 4;
	} else {
		_c_imm16(aopri1.pip);
		_c_imm16(aopri1.pcs);
	}
}
static void WAIT()
{
	setbyte(0x9b);
	avip++;
}
static void PUSHF()
{
	setbyte(0x9c);
	avip++;
}
static void POPF()
{
	setbyte(0x9d);
	avip++;
}
static void SAHF()
{
	setbyte(0x9e);
	avip++;
}
static void LAHF()
{
	setbyte(0x9f);
	avip++;
}
static void MOV_AL_MOFFS8()
{
	setbyte(0xa0);
	if (aopri2.mem == MEM_BP) {
		_SetAddressSize(2);
		_c_imm16(aopri2.disp16);
	} else if (aopri2.mem == MEM_EBP) {
		_SetAddressSize(4);
		_c_imm16(aopri2.disp32);
	} else flagerror = 1;
}
static void MOV_EAX_MOFFS32(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xa1);
	if (aopri2.mem == MEM_BP) {
		_SetAddressSize(2);
		_c_imm16(aopri2.disp16);
	} else if (aopri2.mem == MEM_EBP) {
		_SetAddressSize(4);
		_c_imm16(aopri2.disp32);
	} else flagerror = 1;
}
static void MOV_MOFFS8_AL()
{
	setbyte(0xa2);
	if (aopri1.mem == MEM_BP) {
		_SetAddressSize(2);
		_c_imm16(aopri1.disp16);
	} else if (aopri1.mem == MEM_EBP) {
		_SetAddressSize(4);
		_c_imm16(aopri1.disp32);
	} else flagerror = 1;
}
static void MOV_MOFFS32_EAX(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xa3);
	if (aopri1.mem == MEM_BP) {
		_SetAddressSize(2);
		_c_imm16(aopri1.disp16);
	} else if (aopri1.mem == MEM_EBP) {
		_SetAddressSize(4);
		_c_imm16(aopri1.disp32);
	} else flagerror = 1;
}
static void MOVSB()
{
	setbyte(0xa4);
	rinfo = &aopri2;
	if (rinfo->flagds) rinfo->flagds = 0;
	if (ARG_ESDI8_DSSI8) {
		_SetAddressSize(2);
	} else if (ARG_ESEDI8_DSESI8) {
		_SetAddressSize(4);
	} else flagerror = 1;
}
static void MOVSW(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xa5);
	rinfo = &aopri2;
	if (rinfo->flagds) rinfo->flagds = 0;
	switch (byte) {
	case 2:
		if (ARG_ESDI16_DSSI16) {
			_SetAddressSize(2);
		} else if (ARG_ESEDI16_DSESI16) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	case 4:
		if (ARG_ESDI32_DSSI32) {
			_SetAddressSize(2);
		} else if (ARG_ESEDI32_DSESI32) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	default:flagerror = 1;break;}
}
static void CMPSB()
{
	setbyte(0xa6);
	rinfo = &aopri1;
	if (rinfo->flagds) rinfo->flagds = 0;
	if (ARG_DSSI8_ESDI8) {
		_SetAddressSize(2);
	} else if (ARG_DSESI8_ESEDI8) {
		_SetAddressSize(4);
	} else flagerror = 1;
}
static void CMPSW(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xa7);
	rinfo = &aopri1;
	if (rinfo->flagds) rinfo->flagds = 0;
	switch (byte) {
	case 2:
		if (ARG_DSSI16_ESDI16) {
			_SetAddressSize(2);
		} else if (ARG_DSESI16_ESEDI16) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	case 4:
		if (ARG_DSSI32_ESDI32) {
			_SetAddressSize(2);
		} else if (ARG_DSESI32_ESEDI32) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	default:flagerror = 1;break;}
}
static void TEST_AL_I8()
{
	setbyte(0xa8);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void TEST_AX_I16()
{
	setbyte(0xa9);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void STOSB()
{
	setbyte(0xaa);
	rinfo = NULL;
	if (ARG_ESDI8) {
		_SetAddressSize(2);
	} else if (ARG_ESEDI8) {
		_SetAddressSize(4);
	} else flagerror = 1;
}
static void STOSW(t_nubit8 byte)
{
	setbyte(0xab);
	rinfo = NULL;
	switch (byte) {
	case 2:
		if (ARG_ESDI16) {
			_SetAddressSize(2);
		} else if (ARG_ESEDI16) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	case 4:
		if (ARG_ESDI32) {
			_SetAddressSize(2);
		} else if (ARG_ESEDI32) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	default:flagerror = 1;break;}
}
static void LODSB()
{
	setbyte(0xac);
	rinfo = &aopri1;
	if (rinfo->flagds) rinfo->flagds = 0;
	if (ARG_DSSI8) {
		_SetAddressSize(2);
	} else if (ARG_DSESI8) {
		_SetAddressSize(4);
	} else flagerror = 1;
}
static void LODSW(t_nubit8 byte)
{
	setbyte(0xad);
	rinfo = &aopri1;
	if (rinfo->flagds) rinfo->flagds = 0;
	switch (byte) {
	case 2:
		if (ARG_DSSI16) {
			_SetAddressSize(2);
		} else if (ARG_DSESI16) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	case 4:
		if (ARG_DSSI32) {
			_SetAddressSize(2);
		} else if (ARG_DSESI32) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	default:flagerror = 1;break;}
}
static void SCASB()
{
	setbyte(0xae);
	rinfo = NULL;
	if (ARG_ESDI8) {
		_SetAddressSize(2);
	} else if (ARG_ESEDI8) {
		_SetAddressSize(4);
	} else flagerror = 1;
}
static void SCASW(t_nubit8 byte)
{
	setbyte(0xaf);
	rinfo = NULL;
	switch (byte) {
	case 2:
		if (ARG_ESDI16) {
			_SetAddressSize(2);
		} else if (ARG_ESEDI16) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	case 4:
		if (ARG_ESDI32) {
			_SetAddressSize(2);
		} else if (ARG_ESEDI32) {
			_SetAddressSize(4);
		} else flagerror = 1;
		break;
	default:flagerror = 1;break;}
}
static void MOV_AL_I8()
{
	setbyte(0xb0);
	_c_imm8(aopri2.imm8);
}
static void MOV_CL_I8()
{
	setbyte(0xb1);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_DL_I8()
{
	setbyte(0xb2);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_BL_I8()
{
	setbyte(0xb3);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_AH_I8()
{
	setbyte(0xb4);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_CH_I8()
{
	setbyte(0xb5);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_DH_I8()
{
	setbyte(0xb6);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_BH_I8()
{
	setbyte(0xb7);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void MOV_AX_I16()
{
	setbyte(0xb8);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void MOV_CX_I16()
{
	setbyte(0xb9);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void MOV_DX_I16()
{
	setbyte(0xba);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void MOV_BX_I16()
{
	setbyte(0xbb);
	_c_imm16(aopri2.imm16);
}
static void MOV_SP_I16()
{
	setbyte(0xbc);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void MOV_BP_I16()
{
	setbyte(0xbd);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void MOV_SI_I16()
{
	setbyte(0xbe);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void MOV_DI_I16()
{
	setbyte(0xbf);
	avip++;
	_c_imm16(aopri2.imm16);
}
static void INS_C0(t_nubit8 rid)
{
	setbyte(0xc0);
	_c_modrm(aopri1, rid);
	_c_imm8(aopri2.imm8);
}
static void INS_C1(t_nubit8 rid, t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xc1);
	_c_modrm(aopri1, rid);
	_c_imm8(aopri2.imm8);
}
static void RET_I16()
{
	setbyte(0xc2);
	avip++;
	_c_imm16(aopri1.imm16);
}
static void RET_()
{
	setbyte(0xc3);
	avip++;
}
static void LES_R16_M16()
{
	setbyte(0xc4);
	avip++;
	_c_modrm(aopri2, aopri1.reg16);
}
static void LDS_R16_M16()
{
	setbyte(0xc5);
	avip++;
	_c_modrm(aopri2, aopri1.reg16);
}
static void MOV_M8_I8()
{
	setbyte(0xc6);
	avip++;
	_c_modrm(aopri1, 0x00);
	_c_imm8(aopri2.imm8);
}
static void MOV_M16_I16()
{
	setbyte(0xc7);
	avip++;
	_c_modrm(aopri1, 0x00);
	_c_imm16(aopri2.imm16);
}
static void RETF_I16()
{
	setbyte(0xca);
	avip++;
	_c_imm16(aopri1.imm16);
}
static void RETF_()
{
	setbyte(0xcb);
	avip++;
}
static void INT3()
{
	setbyte(0xcc);
	avip++;
}
static void INT_I8()
{
	setbyte(0xcd);
	avip++;
	_c_imm8(aopri1.imm8);
}
static void INTO()
{
	setbyte(0xcd);
	avip++;
}
static void IRET()
{
	setbyte(0xcf);
	avip++;
}
static void INS_D0(t_nubit8 rid)
{
	setbyte(0xd0);
	avip++;
	_c_modrm(aopri1, rid);
}
static void INS_D1(t_nubit8 rid, t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xd1);
	_c_modrm(aopri1, rid);
}
static void INS_D2(t_nubit8 rid)
{
	setbyte(0xd2);
	avip++;
	_c_modrm(aopri1, rid);
}
static void INS_D3(t_nubit8 rid)
{
	setbyte(0xd3);
	avip++;
	_c_modrm(aopri1, rid);
}
static void AAM()
{
	if (ARG_NONE) {
		setbyte(0xd4);
		avip++;
		_c_imm8(0x0a);
	} else flagerror = 1;
}
static void AAD()
{
	if (ARG_NONE) {
		setbyte(0xd5);
		avip++;
		_c_imm8(0x0a);
	} else flagerror = 1;
}
static void XLAT()
{
	if (ARG_NONE) {
		setbyte(0xd7);
		avip++;
	} else flagerror = 1;
}
static void IN_AL_I8()
{
	setbyte(0xe4);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void IN_AX_I8()
{
	setbyte(0xe5);
	avip++;
	_c_imm8(aopri2.imm8);
}
static void OUT_I8_AL()
{
	setbyte(0xe6);
	avip++;
	_c_imm8(aopri1.imm8);
}
static void OUT_I8_AX()
{
	setbyte(0xe7);
	avip++;
	_c_imm8(aopri1.imm8);
}
static void CALL_REL16()
{
	setbyte(0xe8);
	if (ARG_NEAR_LABEL) {
		labelStoreRef(aopri1.label, PTR_NEAR);
	} else _c_imm16(aopri1.imm16);
}
static void JMP_REL16()
{
	setbyte(0xe9);
	if (ARG_NEAR_LABEL) {
		labelStoreRef(aopri1.label, PTR_NEAR);
	} else _c_imm16(aopri1.imm16);
}
static void JMP_PTR16_16()
{
	setbyte(0xea);
	if (ARG_FAR_LABEL) {
		labelStoreRef(aopri1.label, PTR_FAR);
		iop += 4;
	} else {
		_c_imm16(aopri1.pip);
		_c_imm16(aopri1.pcs);
	}
}
static void IN_AL_DX()
{
	setbyte(0xec);
	avip++;
}
static void IN_AX_DX()
{
	setbyte(0xed);
	avip++;
}
static void OUT_DX_AL()
{
	setbyte(0xee);
	avip++;
}
static void OUT_DX_AX()
{
	setbyte(0xef);
	avip++;
}
static void LOCK()
{
	if (ARG_NONE) {
		setbyte(0xf0);
		avip++;
	} else flagerror = 1;
}
static void QDX()
{
	if (ARG_I8) {
		setbyte(0xf1);
		avip++;
		_c_imm8(aopri1.imm8);
	} else flagerror = 1;
}
static void REPNZ()
{
	flagrepnz = 1;
}
static void REPZ()
{
	flagrepz = 1;
}
static void HLT()
{
	if (ARG_NONE) {
		setbyte(0xf4);
		avip++;
	} else flagerror = 1;
}
static void CMC()
{
	if (ARG_NONE) {
		setbyte(0xf5);
		avip++;
	} else flagerror = 1;
}
static void INS_F6(t_nubit8 rid)
{
	setbyte(0xf6);
	avip++;
	_c_modrm(aopri1, rid);
	if (!rid) _c_imm8(aopri2.imm8);
}
static void INS_F7(t_nubit8 rid)
{
	setbyte(0xf7);
	avip++;
	_c_modrm(aopri1, rid);
	if (!rid) _c_imm16(aopri2.imm16);
}
static void CLC()
{
	if (ARG_NONE) {
		setbyte(0xf8);
		avip++;
	} else flagerror = 1;
}
static void STC()
{
	if (ARG_NONE) {
		setbyte(0xf9);
		avip++;
	} else flagerror = 1;
}
static void CLI()
{
	if (ARG_NONE) {
		setbyte(0xfa);
		avip++;
	} else flagerror = 1;
}
static void STI()
{
	if (ARG_NONE) {
		setbyte(0xfb);
		avip++;
	} else flagerror = 1;
}
static void CLD()
{
	if (ARG_NONE) {
		setbyte(0xfc);
		avip++;
	} else flagerror = 1;
}
static void STD()
{
	if (ARG_NONE) {
		setbyte(0xfd);
		avip++;
	} else flagerror = 1;
}
static void INS_FE(t_nubit8 rid)
{
	setbyte(0xfe);
	_c_modrm(aopri1, rid);
}
static void INS_FF(t_nubit8 rid, t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0xff);
	_c_modrm(aopri1, rid);
}
static void PUSH_FS()
{
	setbyte(0x0f);
	setbyte(0xa0);
}
static void POP_FS()
{
	setbyte(0x0f);
	setbyte(0xa1);
}
static void PUSH_GS()
{
	setbyte(0x0f);
	setbyte(0xa8);
}
static void POP_GS()
{
	setbyte(0x0f);
	setbyte(0xa9);
}
static void INS_0F_01(t_nubit8 rid, t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x0f);
	setbyte(0x01);
	_c_modrm(aopri1, rid);
}
static void MOVZX_R32_RM8(t_nubit8 byte)
{
	_SetOperandSize(byte);
	setbyte(0x0f);
	setbyte(0xb6);
	switch (byte) {
	case 2: _c_modrm(aopri2, aopri1.reg16);break;
	case 4: _c_modrm(aopri2, aopri1.reg32);break;
	default:flagerror = 1;break;}
}
static void MOVZX_R32_RM16()
{
	setbyte(0x0f);
	setbyte(0xb7);
	_c_modrm(aopri2, aopri1.reg32);
}
/* abstract instructions */
static void PUSH()
{
	if      (ARG_ES) PUSH_ES();
	else if (ARG_CS) PUSH_CS();
	else if (ARG_SS) PUSH_SS();
	else if (ARG_DS) PUSH_DS();
	else if (ARG_FS) PUSH_FS();
	else if (ARG_GS) PUSH_GS();
	else if (ARG_AX) PUSH_EAX(2);
	else if (ARG_CX) PUSH_ECX(2);
	else if (ARG_DX) PUSH_EDX(2);
	else if (ARG_BX) PUSH_EBX(2);
	else if (ARG_SP) PUSH_ESP(2);
	else if (ARG_BP) PUSH_EBP(2);
	else if (ARG_SI) PUSH_ESI(2);
	else if (ARG_DI) PUSH_EDI(2);
	else if (ARG_EAX) PUSH_EAX(4);
	else if (ARG_ECX) PUSH_ECX(4);
	else if (ARG_EDX) PUSH_EDX(4);
	else if (ARG_EBX) PUSH_EBX(4);
	else if (ARG_ESP) PUSH_ESP(4);
	else if (ARG_EBP) PUSH_EBP(4);
	else if (ARG_ESI) PUSH_ESI(4);
	else if (ARG_EDI) PUSH_EDI(4);
	else if (ARG_RM16) INS_FF(0x06, 2);
	else if (ARG_RM32) INS_FF(0x06, 4);
	else if (ARG_I8)  PUSH_I8();
	else if (ARG_I16) PUSH_I32(2);
	else if (ARG_I32) PUSH_I32(4);
	else flagerror = 1;
}
static void POP()
{
	if      (ARG_ES) POP_ES();
	else if (ARG_CS) POP_CS();
	else if (ARG_SS) POP_SS();
	else if (ARG_DS) POP_DS();
	else if (ARG_FS) POP_FS();
	else if (ARG_GS) POP_GS();
	else if (ARG_AX) POP_EAX(2);
	else if (ARG_CX) POP_ECX(2);
	else if (ARG_DX) POP_EDX(2);
	else if (ARG_BX) POP_EBX(2);
	else if (ARG_SP) POP_ESP(2);
	else if (ARG_BP) POP_EBP(2);
	else if (ARG_SI) POP_ESI(2);
	else if (ARG_DI) POP_EDI(2);
	else if (ARG_EAX) POP_EAX(4);
	else if (ARG_ECX) POP_ECX(4);
	else if (ARG_EDX) POP_EDX(4);
	else if (ARG_EBX) POP_EBX(4);
	else if (ARG_ESP) POP_ESP(4);
	else if (ARG_EBP) POP_EBP(4);
	else if (ARG_ESI) POP_ESI(4);
	else if (ARG_EDI) POP_EDI(4);
	else if (ARG_RM16) POP_RM32(2);
	else if (ARG_RM32) POP_RM32(4);
	else flagerror = 1;
}
static void ADD()
{
	t_nubit8 rid = 0x00;
	if      (ARG_AL_I8)    ADD_AL_I8();
	else if (ARG_AX_I16)   ADD_EAX_I32(2);
	else if (ARG_EAX_I32)  ADD_EAX_I32(4);
	else if (ARG_R8_RM8)   ADD_R8_RM8();
	else if (ARG_R16_RM16) ADD_R32_RM32(2);
	else if (ARG_R32_RM32) ADD_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   ADD_RM8_R8();
	else if (ARG_RM16_R16) ADD_RM32_R32(2);
	else if (ARG_RM32_R32) ADD_RM32_R32(4);
	else flagerror = 1;
}
static void OR()
{
	t_nubit8 rid = 0x01;
	if      (ARG_AL_I8)    OR_AL_I8();
	else if (ARG_AX_I16)   OR_EAX_I32(2);
	else if (ARG_EAX_I32)  OR_EAX_I32(4);
	else if (ARG_R8_RM8)   OR_R8_RM8();
	else if (ARG_R16_RM16) OR_R32_RM32(2);
	else if (ARG_R32_RM32) OR_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   OR_RM8_R8();
	else if (ARG_RM16_R16) OR_RM32_R32(2);
	else if (ARG_RM32_R32) OR_RM32_R32(4);
	else flagerror = 1;
}
static void ADC()
{
	t_nubit8 rid = 0x02;
	if      (ARG_AL_I8)    ADC_AL_I8();
	else if (ARG_AX_I16)   ADC_EAX_I32(2);
	else if (ARG_EAX_I32)  ADC_EAX_I32(4);
	else if (ARG_R8_RM8)   ADC_R8_RM8();
	else if (ARG_R16_RM16) ADC_R32_RM32(2);
	else if (ARG_R32_RM32) ADC_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   ADC_RM8_R8();
	else if (ARG_RM16_R16) ADC_RM32_R32(2);
	else if (ARG_RM32_R32) ADC_RM32_R32(4);
	else flagerror = 1;
}
static void SBB()
{
	t_nubit8 rid = 0x03;
	if      (ARG_AL_I8)    SBB_AL_I8();
	else if (ARG_AX_I16)   SBB_EAX_I32(2);
	else if (ARG_EAX_I32)  SBB_EAX_I32(4);
	else if (ARG_R8_RM8)   SBB_R8_RM8();
	else if (ARG_R16_RM16) SBB_R32_RM32(2);
	else if (ARG_R32_RM32) SBB_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   SBB_RM8_R8();
	else if (ARG_RM16_R16) SBB_RM32_R32(2);
	else if (ARG_RM32_R32) SBB_RM32_R32(4);
	else flagerror = 1;
}
static void AND()
{
	t_nubit8 rid = 0x04;
	if      (ARG_AL_I8)    AND_AL_I8();
	else if (ARG_AX_I16)   AND_EAX_I32(2);
	else if (ARG_EAX_I32)  AND_EAX_I32(4);
	else if (ARG_R8_RM8)   AND_R8_RM8();
	else if (ARG_R16_RM16) AND_R32_RM32(2);
	else if (ARG_R32_RM32) AND_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   AND_RM8_R8();
	else if (ARG_RM16_R16) AND_RM32_R32(2);
	else if (ARG_RM32_R32) AND_RM32_R32(4);
	else flagerror = 1;
}
static void SUB()
{
	t_nubit8 rid = 0x05;
	if      (ARG_AL_I8)    SUB_AL_I8();
	else if (ARG_AX_I16)   SUB_EAX_I32(2);
	else if (ARG_EAX_I32)  SUB_EAX_I32(4);
	else if (ARG_R8_RM8)   SUB_R8_RM8();
	else if (ARG_R16_RM16) SUB_R32_RM32(2);
	else if (ARG_R32_RM32) SUB_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   SUB_RM8_R8();
	else if (ARG_RM16_R16) SUB_RM32_R32(2);
	else if (ARG_RM32_R32) SUB_RM32_R32(4);
	else flagerror = 1;
}
static void XOR()
{
	t_nubit8 rid = 0x06;
	if      (ARG_AL_I8)    XOR_AL_I8();
	else if (ARG_AX_I16)   XOR_EAX_I32(2);
	else if (ARG_EAX_I32)  XOR_EAX_I32(4);
	else if (ARG_R8_RM8)   XOR_R8_RM8();
	else if (ARG_R16_RM16) XOR_R32_RM32(2);
	else if (ARG_R32_RM32) XOR_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   XOR_RM8_R8();
	else if (ARG_RM16_R16) XOR_RM32_R32(2);
	else if (ARG_RM32_R32) XOR_RM32_R32(4);
	else flagerror = 1;
}
static void CMP()
{
	t_nubit8 rid = 0x07;
	if      (ARG_AL_I8)    CMP_AL_I8();
	else if (ARG_AX_I16)   CMP_EAX_I32(2);
	else if (ARG_EAX_I32)  CMP_EAX_I32(4);
	else if (ARG_R8_RM8)   CMP_R8_RM8();
	else if (ARG_R16_RM16) CMP_R32_RM32(2);
	else if (ARG_R32_RM32) CMP_R32_RM32(4);
	else if (ARG_RM8_I8)   INS_80(rid);
	else if (ARG_RM16_I8) {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = aopri2.imm8;
				INS_81(rid, 2);
			} else INS_83(rid, 2);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 2);
			else {
				aopri2.type = TYPE_I16;
				aopri2.imm16 = 0xff00 | aopri2.imm8;
				INS_81(rid, 2);
			}
	} else if (ARG_RM32_I8)  {
		if (!aopri2.immn)
			if (aopri2.imm8 > 0x7f) {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = aopri2.imm8;
				INS_81(rid, 4);
			} else INS_83(rid, 4);
		else
			if (aopri2.imm8 > 0x7f)
				INS_83(rid, 4);
			else {
				aopri2.type = TYPE_I32;
				aopri2.imm32 = 0xffffff00 | aopri2.imm8;
				INS_81(rid, 4);
			}
	} else if (ARG_RM16_I16) INS_81(rid, 2);
	else if (ARG_RM32_I32) INS_81(rid, 4);
	else if (ARG_RM8_R8)   CMP_RM8_R8();
	else if (ARG_RM16_R16) CMP_RM32_R32(2);
	else if (ARG_RM32_R32) CMP_RM32_R32(4);
	else flagerror = 1;
}
static void INC()
{
	if      (ARG_AX) INC_AX();
	else if (ARG_CX) INC_CX();
	else if (ARG_DX) INC_DX();
	else if (ARG_BX) INC_BX();
	else if (ARG_SP) INC_SP();
	else if (ARG_BP) INC_BP();
	else if (ARG_SI) INC_SI();
	else if (ARG_DI) INC_DI();
	else if (ARG_RM8s) INS_FE(0x00);
	else if (ARG_RM16s) INS_FF(0x00, 2);
	else if (ARG_RM32s) INS_FF(0x00, 4);
	else flagerror = 1;
}
static void DEC()
{
	if      (ARG_AX) DEC_AX();
	else if (ARG_CX) DEC_CX();
	else if (ARG_DX) DEC_DX();
	else if (ARG_BX) DEC_BX();
	else if (ARG_SP) DEC_SP();
	else if (ARG_BP) DEC_BP();
	else if (ARG_SI) DEC_SI();
	else if (ARG_DI) DEC_DI();
	else if (ARG_RM8s) INS_FE(0x01);
	else if (ARG_RM16s) INS_FF(0x01, 2);
	else if (ARG_RM32s) INS_FF(0x01, 4);
	else flagerror = 1;
}
static void JCC_REL(t_nubit8 opcode)
{
	if (ARG_I8s) {
		setbyte(opcode);
		_c_imm8(aopri1.imm8);
	} else if (ARG_I16s) {
		_SetOperandSize(2);
		setbyte(0x0f);
		setbyte(opcode + 0x10);
		_c_imm16(aopri1.imm16);
	} else if (ARG_I32s) {
		_SetOperandSize(4);
		setbyte(0x0f);
		setbyte(opcode + 0x10);
		_c_imm16(aopri1.imm32);
	} else if (ARG_PNONE_LABEL || ARG_SHORT_LABEL) {
		setbyte(opcode);
		avip++;
		labelStoreRef(aopri1.label, PTR_SHORT);
		avip++;
	} else flagerror = 1;
}
static void TEST()
{
	if      (ARG_AL_I8) TEST_AL_I8();
	else if (ARG_AX_I16) TEST_AX_I16();
	else if (ARG_RM8_R8) TEST_RM8_R8();
	else if (ARG_RM16_R16) TEST_RM16_R16();
	else if (ARG_RM8_I8) INS_F6(0x00);
	else if (ARG_RM16_I16) INS_F7(0x00);
	else flagerror = 1;
}
static void XCHG()
{
	if      (ARG_AX_AX) XCHG_AX_AX();
	else if (ARG_CX_AX) XCHG_CX_AX();
	else if (ARG_DX_AX) XCHG_DX_AX();
	else if (ARG_BX_AX) XCHG_BX_AX();
	else if (ARG_SP_AX) XCHG_SP_AX();
	else if (ARG_BP_AX) XCHG_BP_AX();
	else if (ARG_SI_AX) XCHG_SI_AX();
	else if (ARG_DI_AX) XCHG_DI_AX();
	else if (ARG_RM8_R8) XCHG_RM8_R8();
	else if (ARG_RM16_R16) XCHG_RM32_R32(2);
	else if (ARG_RM32_R32) XCHG_RM32_R32(4);
	else flagerror = 1;
}
static void MOV()
{
	if      (ARG_AL_I8) MOV_AL_I8();
	else if (ARG_CL_I8) MOV_CL_I8();
	else if (ARG_DL_I8) MOV_DL_I8();
	else if (ARG_BL_I8) MOV_BL_I8();
	else if (ARG_AH_I8) MOV_AH_I8();
	else if (ARG_CH_I8) MOV_CH_I8();
	else if (ARG_DH_I8) MOV_DH_I8();
	else if (ARG_BH_I8) MOV_BH_I8();
	else if (ARG_AX_I16) MOV_AX_I16();
	else if (ARG_CX_I16) MOV_CX_I16();
	else if (ARG_DX_I16) MOV_DX_I16();
	else if (ARG_BX_I16) MOV_BX_I16();
	else if (ARG_SP_I16) MOV_SP_I16();
	else if (ARG_BP_I16) MOV_BP_I16();
	else if (ARG_SI_I16) MOV_SI_I16();
	else if (ARG_DI_I16) MOV_DI_I16();
	else if (ARG_AL_M8) MOV_AL_MOFFS8();
	else if (ARG_M8_AL) MOV_MOFFS8_AL();
	else if (ARG_AX_M16) MOV_EAX_MOFFS32(2);
	else if (ARG_M16_AX) MOV_MOFFS32_EAX(2);
	else if (ARG_EAX_M32) MOV_EAX_MOFFS32(4);
	else if (ARG_M32_EAX) MOV_MOFFS32_EAX(4);
	else if (ARG_R8_RM8) MOV_R8_RM8();
	else if (ARG_R16_RM16) MOV_R32_RM32(2);
	else if (ARG_R32_RM32) MOV_R32_RM32(4);
	else if (ARG_RM8_R8) MOV_RM8_R8();
	else if (ARG_RM16_R16) MOV_RM32_R32(2);
	else if (ARG_RM32_R32) MOV_RM32_R32(4);
	else if (ARG_RM16_SREG) MOV_RM16_SREG();
	else if (ARG_SREG_RM16) MOV_SREG_RM16();
	else if (ARG_RM8_I8) MOV_M8_I8();
	else if (ARG_RM16_I16) MOV_M16_I16();
	else flagerror = 1;
}
static void LEA()
{
	if (ARG_R16_M16) LEA_R16_M16();
	else flagerror = 1;
}
static void CALL()
{
	if      (ARG_FAR_I16_16 || ARG_FAR_LABEL) CALL_PTR16_16();
	else if (ARG_NEAR_I16s || ARG_NEAR_LABEL || ARG_PNONE_I16s) CALL_REL16();
	else if (ARG_NEAR_RM16s || ARG_PNONE_RM16s) INS_FF(0x02, 2);
	else if (ARG_NEAR_RM32s || ARG_PNONE_RM32s) INS_FF(0x02, 4);
	else if (ARG_FAR_M16_16) INS_FF(0x03, 2);
	else flagerror = 1;
}
static void RET()
{
	if (ARG_I16u) RET_I16();
	else if (ARG_NONE) RET_();
	else flagerror = 1;
}
static void LES()
{
	if (ARG_R16_M16) LES_R16_M16();
	else flagerror = 1;
}
static void LDS()
{
	if (ARG_R16_M16) LDS_R16_M16();
	else flagerror = 1;
}
static void RETF()
{
	if      (ARG_I16u) RETF_I16();
	else if (ARG_NONE) RETF_();
	else flagerror = 1;
}
static void INT()
{
	if (ARG_I8) {
		if (aopri1.imm8 == 0x03) INT3();
		else INT_I8();
	} else flagerror = 1;
}
static void ROL()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x00);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x00, 2);
	else if (ARG_RM8_CL)  INS_D2(0x00);
	else if (ARG_RM16_CL) INS_D3(0x00);
	else if (ARG_RM8_I8)  INS_C0(0x00);
	else if (ARG_RM16_I8) INS_C1(0x00, 2);
	else if (ARG_RM32_I8) INS_C1(0x00, 4);
	else flagerror = 1;
}
static void ROR()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x01);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x01, 2);
	else if (ARG_RM8_CL)  INS_D2(0x01);
	else if (ARG_RM16_CL) INS_D3(0x01);
	else if (ARG_RM8_I8)  INS_C0(0x01);
	else if (ARG_RM16_I8) INS_C1(0x01, 2);
	else if (ARG_RM32_I8) INS_C1(0x01, 4);
	else flagerror = 1;
}
static void RCL()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x02);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x02, 2);
	else if (ARG_RM8_CL)  INS_D2(0x02);
	else if (ARG_RM16_CL) INS_D3(0x02);
	else if (ARG_RM8_I8)  INS_C0(0x02);
	else if (ARG_RM16_I8) INS_C1(0x02, 2);
	else if (ARG_RM32_I8) INS_C1(0x02, 4);
	else flagerror = 1;
}
static void RCR()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x03);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x03, 2);
	else if (ARG_RM8_CL)  INS_D2(0x03);
	else if (ARG_RM16_CL) INS_D3(0x03);
	else if (ARG_RM8_I8)  INS_C0(0x03);
	else if (ARG_RM16_I8) INS_C1(0x03, 2);
	else if (ARG_RM32_I8) INS_C1(0x03, 4);
	else flagerror = 1;
}
static void SHL()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x04);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x04, 2);
	else if (ARG_RM32_I8 && aopri2.imm8 == 1) INS_D1(0x04, 4);
	else if (ARG_RM8_CL)  INS_D2(0x04);
	else if (ARG_RM16_CL) INS_D3(0x04);
	else if (ARG_RM8_I8)  INS_C0(0x04);
	else if (ARG_RM16_I8) INS_C1(0x04, 2);
	else if (ARG_RM32_I8) INS_C1(0x04, 4);
	else flagerror = 1;
}
static void SHR()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x05);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x05, 2);
	else if (ARG_RM32_I8 && aopri2.imm8 == 1) INS_D1(0x05, 4);
	else if (ARG_RM8_CL)  INS_D2(0x05);
	else if (ARG_RM16_CL) INS_D3(0x05);
	else if (ARG_RM8_I8)  INS_C0(0x05);
	else if (ARG_RM16_I8) INS_C1(0x05, 2);
	else if (ARG_RM32_I8) INS_C1(0x05, 4);
	else flagerror = 1;
}
static void SAL()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x04);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x04, 2);
	else if (ARG_RM8_CL)  INS_D2(0x04);
	else if (ARG_RM16_CL) INS_D3(0x04);
	else if (ARG_RM8_I8)  INS_C0(0x04);
	else if (ARG_RM16_I8) INS_C1(0x04, 2);
	else if (ARG_RM32_I8) INS_C1(0x04, 4);
	else flagerror = 1;
}
static void SAR()
{
	if (ARG_RM8_I8 && aopri2.imm8 == 1) INS_D0(0x07);
	else if (ARG_RM16_I8 && aopri2.imm8 == 1) INS_D1(0x07, 2);
	else if (ARG_RM8_CL)  INS_D2(0x07);
	else if (ARG_RM16_CL) INS_D3(0x07);
	else if (ARG_RM8_I8)  INS_C0(0x07);
	else if (ARG_RM16_I8) INS_C1(0x07, 2);
	else if (ARG_RM32_I8) INS_C1(0x07, 4);
	else flagerror = 1;
}
static void LOOPCC(t_nubit8 opcode)
{
	JCC_REL(opcode);
}
static void IN()
{
	if      (ARG_AL_I8u) IN_AL_I8();
	else if (ARG_AX_I8u) IN_AX_I8();
	else if (ARG_AL_DX)  IN_AL_DX();
	else if (ARG_AX_DX)  IN_AX_DX();
	else flagerror = 1;
}
static void OUT()
{
	if      (ARG_I8u_AL) OUT_I8_AL();
	else if (ARG_I8u_AX) OUT_I8_AX();
	else if (ARG_DX_AL)  OUT_DX_AL();
	else if (ARG_DX_AX)  OUT_DX_AX();
	else flagerror = 1;
}
static void NOT()
{
	if      (ARG_RM8s) INS_F6(0x02);
	else if (ARG_RM16s) INS_F7(0x02);
}
static void NEG()
{
	if      (ARG_RM8s) INS_F6(0x03);
	else if (ARG_RM16s) INS_F7(0x03);
}
static void MUL()
{
	if      (ARG_RM8s) INS_F6(0x04);
	else if (ARG_RM16s) INS_F7(0x04);
}
static void IMUL()
{
	if      (ARG_RM8s) INS_F6(0x05);
	else if (ARG_RM16s) INS_F7(0x05);
}
static void DIV()
{
	if      (ARG_RM8s) INS_F6(0x06);
	else if (ARG_RM16s) INS_F7(0x06);
}
static void IDIV()
{
	if      (ARG_RM8s) INS_F6(0x07);
	else if (ARG_RM16s) INS_F7(0x07);
}
static void JMP()
{
	if      (ARG_FAR_I16_16 || ARG_FAR_LABEL) JMP_PTR16_16();
	else if (ARG_SHORT_I8s || ARG_SHORT_LABEL || ARG_PNONE_I8s) JCC_REL(0xeb);
	else if (ARG_NEAR_I16s || ARG_NEAR_LABEL || ARG_PNONE_I16s) JMP_REL16();
	else if (ARG_NEAR_RM16s || ARG_PNONE_RM16s) INS_FF(0x04, 2);
	else if (ARG_NEAR_RM32s || ARG_PNONE_RM32s) INS_FF(0x04, 4);
	else if (ARG_FAR_M16_16) INS_FF(0x05, 2);
	else flagerror = 1;

}
static void LGDT()
{
	if (ARG_M16s) INS_0F_01(0x02, 2);
	else if (ARG_M32) INS_0F_01(0x02, 4);
	else flagerror = 1;
}
static void SMSW()
{
	if (ARG_RM16) INS_0F_01(0x04, 2);
	else if (ARG_R32) INS_0F_01(0x04, 4);
	else flagerror = 1;
}
static void MOVZX()
{
	if (ARG_R16_RM8) MOVZX_R32_RM8(2);
	else if (ARG_R32_RM8) MOVZX_R32_RM8(4);
	else if (ARG_R32_RM16) MOVZX_R32_RM16();
	else flagerror = 1;
}
static void DB()
{
	t_nubitcc i;
	t_aasm_token token;
	token = gettoken(aopr1);
	do {
		switch (token) {
		case TOKEN_PLUS:
			break;
		case TOKEN_MINUS:
			token = gettoken(NULL);
			if (token == TOKEN_IMM8)
				setbyte((~tokimm8) + 1);
			else flagerror = 1;
			avip += 1;
			break;
		case TOKEN_IMM8:
			setbyte(tokimm8);
			avip += 1;
			break;
		case TOKEN_CHAR:
			setbyte(tokchar);
			avip += 1;
			break;
		case TOKEN_STRING:
			for (i = 0;i < strlen(tokstring);++i) {
				setbyte(tokstring[i]);
				avip += 1;
			}
			break;
		case TOKEN_NULL:
		case TOKEN_END:
			return;
			break;
		case TOKEN_IMM16:
		default:
			flagerror = 1;
			break;
		}
		token = gettoken(NULL);
	} while (!flagerror);
}
static void DW()
{
	t_aasm_token token;
	token = gettoken(aopr1);
	do {
		switch (token) {
		case TOKEN_PLUS:
			break;
		case TOKEN_MINUS:
			token = gettoken(NULL);
			if (token == TOKEN_IMM8)
				setword((~((t_nubit16)tokimm8)) + 1);
			else if (token == TOKEN_IMM16)
				setword((~tokimm16) + 1);
			else flagerror = 1;
			avip += 2;
			break;
		case TOKEN_IMM8:
			setword(tokimm8);
			avip += 2;
			break;
		case TOKEN_IMM16:
			setword(tokimm16);
			avip += 2;
			break;
		case TOKEN_NULL:
		case TOKEN_END:
			return;
			break;
		default:
			flagerror = 1;
			break;
		}
		token = gettoken(NULL);
	} while (!flagerror);
}

static void exec()
{
	/* assemble single statement */
	if (!rop || !rop[0]) return;
	if      (!strcmp(rop, "db"))  DB();
	else if (!strcmp(rop, "dw"))  DW();

	else if (!strcmp(rop, "add")) ADD();
	else if (!strcmp(rop,"push")) PUSH();
	else if (!strcmp(rop, "pop")) POP();
	else if (!strcmp(rop, "or" )) OR();
	else if (!strcmp(rop, "adc")) ADC();
	else if (!strcmp(rop, "sbb")) SBB();
	else if (!strcmp(rop, "and")) AND();
	else if (!strcmp(rop, "es:")) ES();
	else if (!strcmp(rop, "daa")) DAA();
	else if (!strcmp(rop, "sub")) SUB();
	else if (!strcmp(rop, "cs:")) CS();
	else if (!strcmp(rop, "das")) DAS();
	else if (!strcmp(rop, "xor")) XOR();
	else if (!strcmp(rop, "ss:")) SS();
	else if (!strcmp(rop, "aaa")) AAA();
	else if (!strcmp(rop, "cmp")) CMP();
	else if (!strcmp(rop, "ds:")) DS();
	else if (!strcmp(rop, "aas")) AAS();
	else if (!strcmp(rop, "inc")) INC();
	else if (!strcmp(rop, "dec")) DEC();
	else if (!strcmp(rop, "jo" )) JCC_REL(0x70);
	else if (!strcmp(rop, "jno")) JCC_REL(0x71);
	else if (!strcmp(rop, "jb" )) JCC_REL(0x72);
	else if (!strcmp(rop, "jc" )) JCC_REL(0x72);
	else if (!strcmp(rop,"jnae")) JCC_REL(0x72);
	else if (!strcmp(rop, "jae")) JCC_REL(0x73);
	else if (!strcmp(rop, "jnb")) JCC_REL(0x73);
	else if (!strcmp(rop, "jnc")) JCC_REL(0x73);
	else if (!strcmp(rop, "je" )) JCC_REL(0x74);
	else if (!strcmp(rop, "jz" )) JCC_REL(0x74);
	else if (!strcmp(rop, "jne")) JCC_REL(0x75);
	else if (!strcmp(rop, "jnz")) JCC_REL(0x75);
	else if (!strcmp(rop, "jbe")) JCC_REL(0x76);
	else if (!strcmp(rop, "jna")) JCC_REL(0x76);
	else if (!strcmp(rop, "ja" )) JCC_REL(0x77);
	else if (!strcmp(rop,"jnbe")) JCC_REL(0x77);
	else if (!strcmp(rop, "js" )) JCC_REL(0x78);
	else if (!strcmp(rop, "jns")) JCC_REL(0x79);
	else if (!strcmp(rop, "jp" )) JCC_REL(0x7a);
	else if (!strcmp(rop, "jpe")) JCC_REL(0x7a);
	else if (!strcmp(rop, "jnp")) JCC_REL(0x7b);
	else if (!strcmp(rop, "jpo")) JCC_REL(0x7b);
	else if (!strcmp(rop, "jl" )) JCC_REL(0x7c);
	else if (!strcmp(rop,"jnge")) JCC_REL(0x7c);
	else if (!strcmp(rop, "jge")) JCC_REL(0x7d);
	else if (!strcmp(rop, "jnl")) JCC_REL(0x7d);
	else if (!strcmp(rop, "jle")) JCC_REL(0x7e);
	else if (!strcmp(rop, "jng")) JCC_REL(0x7e);
	else if (!strcmp(rop, "jg" )) JCC_REL(0x7f);
	else if (!strcmp(rop,"jnle")) JCC_REL(0x7f);
	else if (!strcmp(rop,"test")) TEST();
	else if (!strcmp(rop,"xchg")) XCHG();
	else if (!strcmp(rop, "mov")) MOV();
	else if (!strcmp(rop, "lea")) LEA();
	else if (!strcmp(rop, "nop")) NOP();
	else if (!strcmp(rop, "cbw")) CBW();
	else if (!strcmp(rop, "cwd")) CWD();
	else if (!strcmp(rop,"call")) CALL();
	else if (!strcmp(rop,"wait")) WAIT();
	else if (!strcmp(rop,"pushf")) PUSHF();
	else if (!strcmp(rop,"popf")) POPF();
	else if (!strcmp(rop,"sahf")) SAHF();
	else if (!strcmp(rop,"lahf")) LAHF();
	else if (!strcmp(rop,"movsb")) MOVSB();
	else if (!strcmp(rop,"movsw")) MOVSW(2);
	else if (!strcmp(rop,"movsd")) MOVSW(4);
	else if (!strcmp(rop,"cmpsb")) CMPSB();
	else if (!strcmp(rop,"cmpsw")) CMPSW(2);
	else if (!strcmp(rop,"cmpsd")) CMPSW(4);
	else if (!strcmp(rop,"stosb")) STOSB();
	else if (!strcmp(rop,"stosw")) STOSW(2);
	else if (!strcmp(rop,"stosd")) STOSW(4);
	else if (!strcmp(rop,"lodsb")) LODSB();
	else if (!strcmp(rop,"lodsw")) LODSW(2);
	else if (!strcmp(rop,"lodsd")) LODSW(4);
	else if (!strcmp(rop,"scasb")) SCASB();
	else if (!strcmp(rop,"scasw")) SCASW(2);
	else if (!strcmp(rop,"scasd")) SCASW(4);
	else if (!strcmp(rop, "ret")) RET();
	else if (!strcmp(rop, "les")) LES();
	else if (!strcmp(rop, "lds")) LDS();
	else if (!strcmp(rop,"retf")) RETF();
	else if (!strcmp(rop, "int")) INT();
	else if (!strcmp(rop,"into")) INTO();
	else if (!strcmp(rop,"iret")) IRET();
	else if (!strcmp(rop, "rol")) ROL();
	else if (!strcmp(rop, "ror")) ROR();
	else if (!strcmp(rop, "rcl")) RCL();
	else if (!strcmp(rop, "rcr")) RCR();
	else if (!strcmp(rop, "shl")) SHL();
	else if (!strcmp(rop, "shr")) SHR();
	else if (!strcmp(rop, "sal")) SAL();
	else if (!strcmp(rop, "sar")) SAR();
	else if (!strcmp(rop, "aam")) AAM();
	else if (!strcmp(rop, "aad")) AAD();
	else if (!strcmp(rop,"xlat")) XLAT();
	else if (!strcmp(rop,"loopne")) LOOPCC(0xe0);
	else if (!strcmp(rop,"loopnz")) LOOPCC(0xe0);
	else if (!strcmp(rop,"loope")) LOOPCC(0xe1);
	else if (!strcmp(rop,"loopz")) LOOPCC(0xe1);
	else if (!strcmp(rop,"loop")) LOOPCC(0xe2);
	else if (!strcmp(rop,"jcxz")) JCC_REL(0xe3);
	else if (!strcmp(rop, "in" )) IN();
	else if (!strcmp(rop, "out")) OUT();
	else if (!strcmp(rop, "jmp")) JMP();
	else if (!strcmp(rop, "lock")) LOCK();
	else if (!strcmp(rop, "qdx")) QDX();
	else if (!strcmp(rop,"repne:")) REPNZ();
	else if (!strcmp(rop,"repnz:")) REPNZ();
	else if (!strcmp(rop, "rep:")) REPZ();
	else if (!strcmp(rop,"repe:")) REPZ();
	else if (!strcmp(rop,"repz:")) REPZ();
	else if (!strcmp(rop, "hlt")) HLT();
	else if (!strcmp(rop, "cmc")) CMC();
	else if (!strcmp(rop, "not")) NOT();
	else if (!strcmp(rop, "neg")) NEG();
	else if (!strcmp(rop, "mul")) MUL();
	else if (!strcmp(rop,"imul")) IMUL();
	else if (!strcmp(rop, "div")) DIV();
	else if (!strcmp(rop,"idiv")) IDIV();
	else if (!strcmp(rop, "clc")) CLC();
	else if (!strcmp(rop, "stc")) STC();
	else if (!strcmp(rop, "cli")) CLI();
	else if (!strcmp(rop, "sti")) STI();
	else if (!strcmp(rop, "cld")) CLD();
	else if (!strcmp(rop, "std")) STD();
	else if (rop[0] == '$') LABEL();
	else if (!strcmp(rop, "lgdt"))  LGDT();
	else if (!strcmp(rop, "smsw"))  SMSW();
	else if (!strcmp(rop, "movzx")) MOVZX();
	else flagerror = 1;
//	if (flagerror) printf("exec.flagerror = %d\n",flagerror);
}


static t_bool is_end(char c)
{
	return (!c || c == '\n' || c == ';');
}
static t_bool is_space(char c)
{
	return (c == ' ' || c == '\t');
}
static t_bool is_prefix()
{
	if (!strcmp(rop, "es:") || !strcmp(rop, "cs:") ||
		!strcmp(rop, "ss:") || !strcmp(rop, "ds:") ||
		!strcmp(rop, "fs:") || !strcmp(rop, "gs:") ||
		!strcmp(rop, "lock:") || !strcmp(rop, "rep:") ||
		!strcmp(rop, "repne:") || !strcmp(rop, "repnz:") ||
		!strcmp(rop, "repe:") || !strcmp(rop, "repz:")) return 1;
	return 0;
}
static t_bool is_def_str()
{
	if (!strcmp(rop, "db")) return 1;
	return 0;
}
static t_strptr take_arg(t_strptr s)
{
	static t_strptr rstart = NULL;
	t_strptr rend, rresult;
	if (s) rstart = s;
	if (!rstart) return NULL;
	while (!is_end(*rstart) && is_space(*rstart)) rstart++;
	if (*rstart == ',' || is_end(*rstart)) return NULL;
	rresult = rstart;
	while (!is_end(*rstart) && (*rstart) != ',') rstart++;
	rend = rstart - 1;
	if (is_end(*rstart)) rstart = NULL;
	else rstart++;
	while (!is_end(*rend) && is_space(*rend)) rend--;
	*(rend + 1) = 0;
	return rresult;
}

t_nubit8 aasm32(const t_strptr stmt, t_vaddrcc rcode)
{
	char oldchar;
	t_nubit8 len;
	t_string astmt;
	t_strptr rstmt;
	t_bool flagprefix;

	if (!stmt || !stmt[0] || stmt[0] == '\n') return 0;

	memcpy(astmt, stmt, MAXLINE);
	rstmt = astmt;

	flagopr = flagaddr = 0;
	flaglock = flagrepz = flagrepnz = 0;

	iop = 0;

	/* process prefixes */
	do {
		while (!is_end(*rstmt) && is_space(*rstmt)) rstmt++;
		rop = rstmt;
		while (!is_end(*rstmt) && !is_space(*rstmt)) rstmt++;
		oldchar = *rstmt;
		*rstmt = 0;
		rstmt++;
		lcase(rop);
		flagprefix = is_prefix();
		if (flagprefix) exec();
	} while (flagprefix);

	/* process assembly statement */
	if (!is_def_str()) lcase(rstmt);
	ropr1 = take_arg(rstmt);
	ropr2 = take_arg(NULL);
	ropr3 = take_arg(NULL);

	aopri1 = parsearg(ropr1);
	if (flagerror) vapiPrint("bad operand 1: '%s'\n", ropr1);
	aopri2 = parsearg(ropr2);
	if (flagerror) vapiPrint("bad operand 2: '%s'\n", ropr2);
	aopri3 = parsearg(ropr3);
	if (flagerror) vapiPrint("bad operand 3: '%s'\n", ropr3);

	if (isM(aopri1)) rinfo = &aopri1;
	else if (isM(aopri2)) rinfo = &aopri2;
	else if (isM(aopri3)) rinfo = &aopri3;
	else rinfo = NULL;

	exec();

	//	vapiPrint("[%s] [%s/%d] [%s/%d] [%s/%d]\n", rop, ropr1, aopri1.type, ropr2, aopri2.type, ropr3, aopri3.type);
	if (flagerror) {
		len = 0;
		vapiPrint("bad instruction: '%s'\n", stmt);
		vapiPrint("[%s] [%s/%d] [%s/%d] [%s/%d]\n", rop, ropr1, aopri1.type, ropr2, aopri2.type, ropr3, aopri3.type);
	} else {
		len = 0;
		if (rinfo && rinfo->flages) {d_nubit8(rcode + len) = 0x26;len++;}
		if (rinfo && rinfo->flagcs) {d_nubit8(rcode + len) = 0x2e;len++;}
		if (rinfo && rinfo->flagss) {d_nubit8(rcode + len) = 0x36;len++;}
		if (rinfo && rinfo->flagds) {d_nubit8(rcode + len) = 0x3e;len++;}
		if (rinfo && rinfo->flagfs) {d_nubit8(rcode + len) = 0x64;len++;}
		if (rinfo && rinfo->flaggs) {d_nubit8(rcode + len) = 0x65;len++;}
		if (flaglock) {d_nubit8(rcode + len) = 0xf0;len++;}
		if (flagrepnz) {d_nubit8(rcode + len) = 0xf2;len++;}
		if (flagrepz) {d_nubit8(rcode + len) = 0xf3;len++;}
		if (flagaddr) {d_nubit8(rcode + len) = 0x67;len++;}
		if (flagopr) {d_nubit8(rcode + len) = 0x66;len++;}
		memcpy((void *)(rcode + len), (void *)acode, iop);
		len += iop;
	}
	return len;
}