//===-- SM83MCCodeEmitter.cpp - SM83 Code Emitter -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the SM83MCCodeEmitter class, which converts SM83
// MachineInstr objects into their binary encoding.
//
// SM83 instruction encoding reference:
//   https://gbdev.io/pandocs/CPU_Instruction_Set.html
//   https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
//
// Register field encoding (same as HWEncoding in SM83RegisterInfo.td):
//   B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7
//
// Register pair field encoding (for most pair ops):
//   BC=0, DE=1, HL=2, SP=3
// PUSH/POP pair encoding:
//   BC=0, DE=1, HL=2, AF=3
//
//===----------------------------------------------------------------------===//

#include "SM83MCCodeEmitter.h"
#include "SM83FixupKinds.h"
#include "SM83MCTargetDesc.h"

#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "sm83-mccodeemitter"

namespace llvm {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline void emit8(SmallVectorImpl<char> &CB, uint8_t Byte) {
  CB.push_back(static_cast<char>(Byte));
}

// Get the SM83 8-bit register field (0-7) from a physical register number.
static uint8_t getReg8Enc(unsigned Reg, MCContext &Ctx) {
  return Ctx.getRegisterInfo()->getEncodingValue(Reg);
}

// Get the SM83 16-bit pair field for most pair ops (BC=0, DE=1, HL=2, SP=3).
// SP has HWEncoding=4 in our register file but encodes as 3 in pair fields.
static uint8_t getPairEnc(unsigned Reg, MCContext &Ctx) {
  uint8_t HW = Ctx.getRegisterInfo()->getEncodingValue(Reg);
  return (HW == 4) ? 3 : HW; // remap SP(4) → 3
}

// ---------------------------------------------------------------------------
// Emit helpers
// ---------------------------------------------------------------------------

void SM83MCCodeEmitter::emitImm16(const MCOperand &MO, uint32_t FixupOffset,
                                   SmallVectorImpl<char> &CB,
                                   SmallVectorImpl<MCFixup> &Fixups) const {
  if (MO.isImm()) {
    uint16_t Val = static_cast<uint16_t>(MO.getImm());
    CB.push_back(Val & 0xFF);
    CB.push_back((Val >> 8) & 0xFF);
    return;
  }
  assert(MO.isExpr() && "Expected expr operand for 16-bit address");
  Fixups.push_back(MCFixup::create(FixupOffset, MO.getExpr(),
                                   (MCFixupKind)SM83::fixup_16));
  CB.push_back(0);
  CB.push_back(0);
}

void SM83MCCodeEmitter::emitPCRel8(const MCOperand &MO, uint32_t FixupOffset,
                                    SmallVectorImpl<char> &CB,
                                    SmallVectorImpl<MCFixup> &Fixups) const {
  if (MO.isImm()) {
    CB.push_back(static_cast<uint8_t>(MO.getImm()));
    return;
  }
  assert(MO.isExpr() && "Expected expr operand for PCRel8");
  Fixups.push_back(MCFixup::create(FixupOffset, MO.getExpr(),
                                   (MCFixupKind)SM83::fixup_pcrel_8,
                                   /*IsPCRel=*/true));
  CB.push_back(0);
}

// ---------------------------------------------------------------------------
// encodeInstruction — main dispatch
// ---------------------------------------------------------------------------

void SM83MCCodeEmitter::encodeInstruction(const MCInst &MI,
                                          SmallVectorImpl<char> &CB,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  unsigned Opc = MI.getOpcode();

  switch (Opc) {
  // -----------------------------------------------------------------------
  // Misc 1-byte instructions
  // -----------------------------------------------------------------------
  case SM83::NOP:  emit8(CB, 0x00); return;
  case SM83::HALT: emit8(CB, 0x76); return;
  case SM83::DI:   emit8(CB, 0xF3); return;
  case SM83::EI:   emit8(CB, 0xFB); return;
  case SM83::DAA:  emit8(CB, 0x27); return;
  case SM83::CPL:  emit8(CB, 0x2F); return;
  case SM83::SCF:  emit8(CB, 0x37); return;
  case SM83::CCF:  emit8(CB, 0x3F); return;
  case SM83::RLCA: emit8(CB, 0x07); return;
  case SM83::RRCA: emit8(CB, 0x0F); return;
  case SM83::RLA:  emit8(CB, 0x17); return;
  case SM83::RRA:  emit8(CB, 0x1F); return;

  // STOP is a 2-byte instruction: 0x10 0x00
  case SM83::STOP:
    emit8(CB, 0x10);
    emit8(CB, 0x00);
    return;

  // -----------------------------------------------------------------------
  // 8-bit register-to-register load: LD r, r'
  // Encoding: 01 ddd sss
  // -----------------------------------------------------------------------
  case SM83::LDrr: {
    uint8_t Dst = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    uint8_t Src = getReg8Enc(MI.getOperand(1).getReg(), Ctx);
    emit8(CB, 0x40 | (Dst << 3) | Src);
    return;
  }

  // -----------------------------------------------------------------------
  // 8-bit load immediate: LD r, n
  // Encoding: 00 rrr 110  n
  // -----------------------------------------------------------------------
  case SM83::LDri: {
    uint8_t Dst = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (Dst << 3) | 0x06);
    emit8(CB, static_cast<uint8_t>(MI.getOperand(1).getImm()));
    return;
  }

  // -----------------------------------------------------------------------
  // 8-bit load from (HL): LD r, (HL)
  // Encoding: 01 rrr 110   (src field = 6 = (HL))
  // -----------------------------------------------------------------------
  case SM83::LDrHL: {
    uint8_t Dst = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, 0x40 | (Dst << 3) | 0x06);
    return;
  }

  // -----------------------------------------------------------------------
  // 8-bit store to (HL): LD (HL), r
  // Encoding: 01 110 sss
  // -----------------------------------------------------------------------
  case SM83::LDHLr: {
    uint8_t Src = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, 0x70 | Src);
    return;
  }

  // -----------------------------------------------------------------------
  // 8-bit store immediate to (HL): LD (HL), n
  // Encoding: 0x36  n
  // -----------------------------------------------------------------------
  case SM83::LDHLi:
    emit8(CB, 0x36);
    emit8(CB, static_cast<uint8_t>(MI.getOperand(0).getImm()));
    return;

  // -----------------------------------------------------------------------
  // Indirect loads/stores through specific pairs
  // -----------------------------------------------------------------------
  case SM83::LDA_BC:  emit8(CB, 0x0A); return;
  case SM83::LDA_DE:  emit8(CB, 0x1A); return;
  case SM83::LDA_HLI: emit8(CB, 0x2A); return;
  case SM83::LDA_HLD: emit8(CB, 0x3A); return;
  case SM83::LDBC_A:  emit8(CB, 0x02); return;
  case SM83::LDDE_A:  emit8(CB, 0x12); return;
  case SM83::LDHLI_A: emit8(CB, 0x22); return;
  case SM83::LDHLD_A: emit8(CB, 0x32); return;

  // -----------------------------------------------------------------------
  // Direct memory loads/stores: LD A,(nn) / LD (nn),A
  // -----------------------------------------------------------------------
  case SM83::LDA_nn:
    emit8(CB, 0xFA);
    emitImm16(MI.getOperand(0), 1, CB, Fixups);
    return;
  case SM83::LDnn_A:
    emit8(CB, 0xEA);
    emitImm16(MI.getOperand(0), 1, CB, Fixups);
    return;

  // -----------------------------------------------------------------------
  // High-page loads/stores
  // -----------------------------------------------------------------------
  case SM83::LDH_An:
    emit8(CB, 0xF0);
    emit8(CB, static_cast<uint8_t>(MI.getOperand(0).getImm()));
    return;
  case SM83::LDH_nA:
    emit8(CB, 0xE0);
    emit8(CB, static_cast<uint8_t>(MI.getOperand(0).getImm()));
    return;
  case SM83::LDH_AC: emit8(CB, 0xF2); return;
  case SM83::LDH_CA: emit8(CB, 0xE2); return;

  // -----------------------------------------------------------------------
  // 16-bit load immediate: LD rr, nn
  // Encoding: 00 rr0 001  lo  hi   (pair: BC=0,DE=1,HL=2,SP=3)
  // -----------------------------------------------------------------------
  case SM83::LDrri: {
    uint8_t Pair = getPairEnc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (Pair << 4) | 0x01);
    emitImm16(MI.getOperand(1), 1, CB, Fixups);
    return;
  }

  // LD SP, HL — copy HL to SP
  case SM83::LDSPHL:
    emit8(CB, 0xF9);
    return;

  // LD (nn), SP
  case SM83::LDnn_SP:
    emit8(CB, 0x08);
    emitImm16(MI.getOperand(0), 1, CB, Fixups);
    return;

  // LD HL, SP+n — F8 + signed imm8
  case SM83::LDHLSP:
    emit8(CB, 0xF8);
    emit8(CB, static_cast<uint8_t>(MI.getOperand(0).getImm()));
    return;

  // -----------------------------------------------------------------------
  // PUSH / POP
  // Encoding: PUSH = 11 rr0 101, POP = 11 rr0 001
  // Pair encoding: BC=0, DE=1, HL=2, AF=3 (HWEncoding direct for GPR16)
  // -----------------------------------------------------------------------
  case SM83::PUSH: {
    // GPR16: BC(0), DE(1), HL(2), AF(3) — HWEncoding maps directly
    uint8_t Pair = Ctx.getRegisterInfo()->getEncodingValue(MI.getOperand(0).getReg());
    emit8(CB, 0xC5 | (Pair << 4));
    return;
  }
  case SM83::POP: {
    uint8_t Pair = Ctx.getRegisterInfo()->getEncodingValue(MI.getOperand(0).getReg());
    emit8(CB, 0xC1 | (Pair << 4));
    return;
  }

  // -----------------------------------------------------------------------
  // 8-bit ALU: ADD/ADC/SUB/SBC/AND/OR/XOR/CP A, r
  // Encoding: 10 ooo rrr  (ooo: ADD=0,ADC=1,SUB=2,SBC=3,AND=4,XOR=5,OR=6,CP=7)
  // -----------------------------------------------------------------------
  case SM83::ADDr: emit8(CB, 0x80 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::ADCr: emit8(CB, 0x88 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::SUBr: emit8(CB, 0x90 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::SBCr: emit8(CB, 0x98 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::ANDr: emit8(CB, 0xA0 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::XORr: emit8(CB, 0xA8 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::ORr:  emit8(CB, 0xB0 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;
  case SM83::CPr:  emit8(CB, 0xB8 | getReg8Enc(MI.getOperand(0).getReg(), Ctx)); return;

  // -----------------------------------------------------------------------
  // 8-bit ALU with immediate
  // -----------------------------------------------------------------------
  case SM83::ADDi: emit8(CB, 0xC6); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::ADCi: emit8(CB, 0xCE); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::SUBi: emit8(CB, 0xD6); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::SBCi: emit8(CB, 0xDE); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::ANDi: emit8(CB, 0xE6); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::XORi: emit8(CB, 0xEE); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::ORi:  emit8(CB, 0xF6); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;
  case SM83::CPi:  emit8(CB, 0xFE); emit8(CB, (uint8_t)MI.getOperand(0).getImm()); return;

  // -----------------------------------------------------------------------
  // 8-bit ALU with (HL)
  // -----------------------------------------------------------------------
  case SM83::ADDm: emit8(CB, 0x86); return;
  case SM83::ADCm: emit8(CB, 0x8E); return;
  case SM83::SUBm: emit8(CB, 0x96); return;
  case SM83::SBCm: emit8(CB, 0x9E); return;
  case SM83::ANDm: emit8(CB, 0xA6); return;
  case SM83::XORm: emit8(CB, 0xAE); return;
  case SM83::ORm:  emit8(CB, 0xB6); return;
  case SM83::CPm:  emit8(CB, 0xBE); return;

  // -----------------------------------------------------------------------
  // 16-bit ADD HL, rr
  // Encoding: 00 rr1 001  (pair: BC=0,DE=1,HL=2,SP=3)
  // -----------------------------------------------------------------------
  case SM83::ADDHLrr: {
    uint8_t Pair = getPairEnc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (Pair << 4) | 0x09);
    return;
  }

  // ADD SP, signed imm8
  case SM83::ADDSPi:
    emit8(CB, 0xE8);
    emit8(CB, static_cast<uint8_t>(MI.getOperand(0).getImm()));
    return;

  // -----------------------------------------------------------------------
  // INC / DEC 8-bit
  // Encoding: INC = 00 rrr 100, DEC = 00 rrr 101
  // -----------------------------------------------------------------------
  case SM83::INCr: {
    // Tied constraint: op[0]=dst=src; use op[0]
    uint8_t R = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (R << 3) | 0x04);
    return;
  }
  case SM83::DECr: {
    uint8_t R = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (R << 3) | 0x05);
    return;
  }
  case SM83::INCm: emit8(CB, 0x34); return;
  case SM83::DECm: emit8(CB, 0x35); return;

  // -----------------------------------------------------------------------
  // INC / DEC 16-bit pairs (GR16: BC,DE,HL only — no SP)
  // Encoding: INC rr = 00 rr0 011, DEC rr = 00 rr1 011
  // -----------------------------------------------------------------------
  case SM83::INCrr: {
    uint8_t Pair = getPairEnc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (Pair << 4) | 0x03);
    return;
  }
  case SM83::DECrr: {
    uint8_t Pair = getPairEnc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, (Pair << 4) | 0x0B);
    return;
  }
  case SM83::INCSP: emit8(CB, 0x33); return;
  case SM83::DECSP: emit8(CB, 0x3B); return;

  // -----------------------------------------------------------------------
  // CB-prefix: Rotate/Shift on register
  // Encoding: 0xCB  (op_byte | reg_enc)
  // RLC=0x00, RRC=0x08, RL=0x10, RR=0x18, SLA=0x20, SRA=0x28, SWAP=0x30, SRL=0x38
  // -----------------------------------------------------------------------
  case SM83::RLCr: emit8(CB,0xCB); emit8(CB, 0x00|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::RRCr: emit8(CB,0xCB); emit8(CB, 0x08|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::RLr:  emit8(CB,0xCB); emit8(CB, 0x10|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::RRr:  emit8(CB,0xCB); emit8(CB, 0x18|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::SLAr: emit8(CB,0xCB); emit8(CB, 0x20|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::SRAr: emit8(CB,0xCB); emit8(CB, 0x28|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::SWAPr:emit8(CB,0xCB); emit8(CB, 0x30|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;
  case SM83::SRLr: emit8(CB,0xCB); emit8(CB, 0x38|getReg8Enc(MI.getOperand(0).getReg(),Ctx)); return;

  // CB-prefix: Rotate/Shift on (HL) — reg field = 6
  case SM83::RLCm:  emit8(CB,0xCB); emit8(CB, 0x06); return;
  case SM83::RRCm:  emit8(CB,0xCB); emit8(CB, 0x0E); return;
  case SM83::RLm:   emit8(CB,0xCB); emit8(CB, 0x16); return;
  case SM83::RRm:   emit8(CB,0xCB); emit8(CB, 0x1E); return;
  case SM83::SLAm:  emit8(CB,0xCB); emit8(CB, 0x26); return;
  case SM83::SRAm:  emit8(CB,0xCB); emit8(CB, 0x2E); return;
  case SM83::SWAPm: emit8(CB,0xCB); emit8(CB, 0x36); return;
  case SM83::SRLm:  emit8(CB,0xCB); emit8(CB, 0x3E); return;

  // -----------------------------------------------------------------------
  // CB-prefix: BIT b, r
  // Encoding: 0xCB  (0x40 | (bit << 3) | reg)
  // Op[0]=bit_imm, Op[1]=src_reg
  // -----------------------------------------------------------------------
  case SM83::BITr: {
    uint8_t Bit = (uint8_t)MI.getOperand(0).getImm();
    uint8_t R   = getReg8Enc(MI.getOperand(1).getReg(), Ctx);
    emit8(CB, 0xCB);
    emit8(CB, 0x40 | (Bit << 3) | R);
    return;
  }
  case SM83::BITm: {
    uint8_t Bit = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0xCB);
    emit8(CB, 0x40 | (Bit << 3) | 0x06);
    return;
  }

  // -----------------------------------------------------------------------
  // CB-prefix: RES b, r / SET b, r
  // SETr: (outs GPR8:$dst), (ins i8imm:$bit, GPR8:$src), constraint "$src=$dst"
  //   Op[0]=dst, Op[1]=bit, Op[2]=src
  // RESr: same layout
  // -----------------------------------------------------------------------
  case SM83::SETr: {
    uint8_t Bit = (uint8_t)MI.getOperand(1).getImm();
    uint8_t R   = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, 0xCB);
    emit8(CB, 0xC0 | (Bit << 3) | R);
    return;
  }
  case SM83::RESr: {
    uint8_t Bit = (uint8_t)MI.getOperand(1).getImm();
    uint8_t R   = getReg8Enc(MI.getOperand(0).getReg(), Ctx);
    emit8(CB, 0xCB);
    emit8(CB, 0x80 | (Bit << 3) | R);
    return;
  }
  case SM83::SETm: {
    uint8_t Bit = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0xCB);
    emit8(CB, 0xC0 | (Bit << 3) | 0x06);
    return;
  }
  case SM83::RESm: {
    uint8_t Bit = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0xCB);
    emit8(CB, 0x80 | (Bit << 3) | 0x06);
    return;
  }

  // -----------------------------------------------------------------------
  // Control flow: JP / JR / CALL / RET / RST
  // -----------------------------------------------------------------------

  // Unconditional JP nn (3 bytes)
  case SM83::JP:
    emit8(CB, 0xC3);
    emitImm16(MI.getOperand(0), 1, CB, Fixups);
    return;

  // Unconditional JR e (2 bytes)
  case SM83::JR:
    emit8(CB, 0x18);
    emitPCRel8(MI.getOperand(0), 1, CB, Fixups);
    return;

  // Conditional JPcc: Op[0]=cc, Op[1]=target
  // Encoding: 0xC2 | (cc << 3)   (NZ=0,Z=1,NC=2,C=3)
  case SM83::JPcc: {
    uint8_t CC = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0xC2 | (CC << 3));
    emitImm16(MI.getOperand(1), 1, CB, Fixups);
    return;
  }

  // Conditional JRcc: Op[0]=cc, Op[1]=target
  // Encoding: 0x20 | (cc << 3)
  case SM83::JRcc: {
    uint8_t CC = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0x20 | (CC << 3));
    emitPCRel8(MI.getOperand(1), 1, CB, Fixups);
    return;
  }

  // JP HL
  case SM83::JPHL: emit8(CB, 0xE9); return;

  // CALL nn (3 bytes)
  // Note: CALL has variable_ops for argument regs; only Op[0] is the target.
  case SM83::CALL:
    emit8(CB, 0xCD);
    emitImm16(MI.getOperand(0), 1, CB, Fixups);
    return;

  // Conditional CALLcc: Op[0]=cc, Op[1]=target
  // Encoding: 0xC4 | (cc << 3)
  case SM83::CALLcc: {
    uint8_t CC = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0xC4 | (CC << 3));
    emitImm16(MI.getOperand(1), 1, CB, Fixups);
    return;
  }

  // Unconditional RET
  case SM83::RET:  emit8(CB, 0xC9); return;
  case SM83::RETI: emit8(CB, 0xD9); return;

  // Conditional RETcc: Op[0]=cc
  // Encoding: 0xC0 | (cc << 3)
  case SM83::RETcc: {
    uint8_t CC = (uint8_t)MI.getOperand(0).getImm();
    emit8(CB, 0xC0 | (CC << 3));
    return;
  }

  // RST vec — Op[0]=target_address (one of 0,8,16,24,32,40,48,56)
  case SM83::RST: {
    uint8_t Vec = (uint8_t)(MI.getOperand(0).getImm() & 0x38);
    emit8(CB, 0xC7 | Vec);
    return;
  }

  default:
    llvm_unreachable("Unhandled opcode in SM83MCCodeEmitter::encodeInstruction");
  }
}

MCCodeEmitter *createSM83MCCodeEmitter(const MCInstrInfo &MCII,
                                       MCContext &Ctx) {
  return new SM83MCCodeEmitter(MCII, Ctx);
}

} // end namespace llvm
