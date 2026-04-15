//===-- SM83Disassembler.cpp - Disassembler for SM83 ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the SM83 disassembler.  Instructions are decoded
// manually (no TableGen-generated decoder tables) since the SM83 instruction
// encodings are not represented in the .td bit fields.
//
// SM83 instruction encoding reference:
//   https://gbdev.io/pandocs/CPU_Instruction_Set.html
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SM83MCTargetDesc.h"
#include "TargetInfo/SM83TargetInfo.h"

#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define DEBUG_TYPE "sm83-disassembler"

using namespace llvm;

typedef MCDisassembler::DecodeStatus DecodeStatus;

// ============================================================================
// Register decode tables
// SM83 register field encoding: B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7
// ============================================================================
static const MCRegister Reg8Table[8] = {
    SM83::B,  // 0
    SM83::C,  // 1
    SM83::D,  // 2
    SM83::E,  // 3
    SM83::H,  // 4
    SM83::L,  // 5
    0,         // 6 = (HL) indirect — handled separately
    SM83::A,  // 7
};

// Pair encoding for most ops (ADD HL,rr / INC rr / DEC rr / LD rr,nn):
// BC=0, DE=1, HL=2, SP=3
static const MCRegister Reg16Table[4] = {
    SM83::BC,  // 0
    SM83::DE,  // 1
    SM83::HL,  // 2
    SM83::SP,  // 3
};

// PUSH/POP pair encoding: BC=0, DE=1, HL=2, AF=3
static const MCRegister PushPopTable[4] = {
    SM83::BC,  // 0
    SM83::DE,  // 1
    SM83::HL,  // 2
    SM83::AF,  // 3
};

// ============================================================================
// SM83Disassembler class
// ============================================================================
namespace {
class SM83Disassembler : public MCDisassembler {
public:
  SM83Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createSM83Disassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new SM83Disassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSM83Disassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheSM83Target(),
                                         createSM83Disassembler);
}

// ============================================================================
// Helpers
// ============================================================================

// Add an 8-bit register operand from the 3-bit field.
static DecodeStatus addReg8(MCInst &Inst, uint8_t Field) {
  if (Field == 6) return MCDisassembler::Fail; // (HL) — caller handles
  if (Field >= 8) return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(Reg8Table[Field]));
  return MCDisassembler::Success;
}

// Add a 16-bit register pair operand.
static DecodeStatus addReg16(MCInst &Inst, uint8_t Field) {
  if (Field >= 4) return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(Reg16Table[Field]));
  return MCDisassembler::Success;
}

// Add a PUSH/POP register pair operand.
static DecodeStatus addPushPop(MCInst &Inst, uint8_t Field) {
  if (Field >= 4) return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(PushPopTable[Field]));
  return MCDisassembler::Success;
}

// ============================================================================
// Main decode function
// ============================================================================
DecodeStatus SM83Disassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CStream) const {
  if (Bytes.empty()) {
    Size = 0;
    return Fail;
  }

  uint8_t B0 = Bytes[0];

  // -------------------------------------------------------------------------
  // CB-prefix instructions (2-byte opcode)
  // -------------------------------------------------------------------------
  if (B0 == 0xCB) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    uint8_t B1 = Bytes[1];
    uint8_t SubOp = (B1 >> 3) & 0x1F; // top 5 bits of B1
    uint8_t Reg   = B1 & 0x07;
    bool IsHL = (Reg == 6);
    Size = 2;

    if ((B1 & 0xC0) == 0x40) {
      // BIT b, r / BIT b, (HL)
      uint8_t Bit = (B1 >> 3) & 0x07;
      Instr.setOpcode(IsHL ? SM83::BITm : SM83::BITr);
      Instr.addOperand(MCOperand::createImm(Bit));
      if (!IsHL) { if (addReg8(Instr, Reg) == Fail) return Fail; }
      return Success;
    }
    if ((B1 & 0xC0) == 0x80) {
      // RES b, r
      uint8_t Bit = (B1 >> 3) & 0x07;
      if (IsHL) {
        Instr.setOpcode(SM83::RESm);
        Instr.addOperand(MCOperand::createImm(Bit));
      } else {
        Instr.setOpcode(SM83::RESr);
        // SETr/RESr: (outs GPR8:$dst), (ins i8imm:$bit, GPR8:$src)
        // For decoding, produce: dst=Reg, bit=Bit, src=Reg (tied)
        Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
        Instr.addOperand(MCOperand::createImm(Bit));
        Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
      }
      return Success;
    }
    if ((B1 & 0xC0) == 0xC0) {
      // SET b, r
      uint8_t Bit = (B1 >> 3) & 0x07;
      if (IsHL) {
        Instr.setOpcode(SM83::SETm);
        Instr.addOperand(MCOperand::createImm(Bit));
      } else {
        Instr.setOpcode(SM83::SETr);
        Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
        Instr.addOperand(MCOperand::createImm(Bit));
        Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
      }
      return Success;
    }
    // Rotate/shift group (B1 & 0xC0 == 0x00)
    static const unsigned CBOpcTable[8][2] = {
        // [SubOp >> 0][IsHL]
        {SM83::RLCr, SM83::RLCm},   // 0x00: RLC
        {SM83::RRCr, SM83::RRCm},   // 0x08: RRC
        {SM83::RLr,  SM83::RLm},    // 0x10: RL
        {SM83::RRr,  SM83::RRm},    // 0x18: RR
        {SM83::SLAr, SM83::SLAm},   // 0x20: SLA
        {SM83::SRAr, SM83::SRAm},   // 0x28: SRA
        {SM83::SWAPr,SM83::SWAPm},  // 0x30: SWAP
        {SM83::SRLr, SM83::SRLm},   // 0x38: SRL
    };
    uint8_t Grp = (B1 >> 3) & 0x07;
    Instr.setOpcode(CBOpcTable[Grp][IsHL]);
    if (!IsHL) {
      // Tied output+input
      Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
      Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
    }
    return Success;
  }

  // -------------------------------------------------------------------------
  // Main opcode table
  // -------------------------------------------------------------------------

  // --- 00 block ---
  if (B0 == 0x00) { Instr.setOpcode(SM83::NOP); Size = 1; return Success; }

  // LD rr, nn (00 rr0 001)
  if ((B0 & 0xCF) == 0x01) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3;
    Instr.setOpcode(SM83::LDrri);
    uint8_t Pair = (B0 >> 4) & 0x03;
    Instr.addOperand(MCOperand::createReg(Reg16Table[Pair]));
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8)));
    return Success;
  }

  // LD (BC),A = 0x02
  if (B0 == 0x02) { Instr.setOpcode(SM83::LDBC_A); Size = 1; return Success; }
  // LD (DE),A = 0x12
  if (B0 == 0x12) { Instr.setOpcode(SM83::LDDE_A); Size = 1; return Success; }
  // LD (HL+),A = 0x22
  if (B0 == 0x22) { Instr.setOpcode(SM83::LDHLI_A); Size = 1; return Success; }
  // LD (HL-),A = 0x32
  if (B0 == 0x32) { Instr.setOpcode(SM83::LDHLD_A); Size = 1; return Success; }
  // LD A,(BC) = 0x0A
  if (B0 == 0x0A) { Instr.setOpcode(SM83::LDA_BC); Size = 1; return Success; }
  // LD A,(DE) = 0x1A
  if (B0 == 0x1A) { Instr.setOpcode(SM83::LDA_DE); Size = 1; return Success; }
  // LD A,(HL+) = 0x2A
  if (B0 == 0x2A) { Instr.setOpcode(SM83::LDA_HLI); Size = 1; return Success; }
  // LD A,(HL-) = 0x3A
  if (B0 == 0x3A) { Instr.setOpcode(SM83::LDA_HLD); Size = 1; return Success; }

  // INC rr (00 rr0 011)
  if ((B0 & 0xCF) == 0x03) {
    Size = 1;
    uint8_t Pair = (B0 >> 4) & 0x03;
    if (Pair == 3) { Instr.setOpcode(SM83::INCSP); return Success; }
    Instr.setOpcode(SM83::INCrr);
    Instr.addOperand(MCOperand::createReg(Reg16Table[Pair]));
    Instr.addOperand(MCOperand::createReg(Reg16Table[Pair]));
    return Success;
  }

  // DEC rr (00 rr1 011)
  if ((B0 & 0xCF) == 0x0B) {
    Size = 1;
    uint8_t Pair = (B0 >> 4) & 0x03;
    if (Pair == 3) { Instr.setOpcode(SM83::DECSP); return Success; }
    Instr.setOpcode(SM83::DECrr);
    Instr.addOperand(MCOperand::createReg(Reg16Table[Pair]));
    Instr.addOperand(MCOperand::createReg(Reg16Table[Pair]));
    return Success;
  }

  // INC r (00 rrr 100) — only valid in the 0x00-0x3F (00-prefix) range
  if (B0 < 0x40 && (B0 & 0x07) == 0x04) {
    uint8_t Reg = (B0 >> 3) & 0x07;
    if (Reg == 6) { Instr.setOpcode(SM83::INCm); Size = 1; return Success; }
    if (Reg >= 8) { Size = 1; return Fail; }
    Instr.setOpcode(SM83::INCr);
    Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
    Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
    Size = 1;
    return Success;
  }

  // DEC r (00 rrr 101) — only valid in the 0x00-0x3F (00-prefix) range
  if (B0 < 0x40 && (B0 & 0x07) == 0x05) {
    uint8_t Reg = (B0 >> 3) & 0x07;
    if (Reg == 6) { Instr.setOpcode(SM83::DECm); Size = 1; return Success; }
    Instr.setOpcode(SM83::DECr);
    Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
    Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
    Size = 1;
    return Success;
  }

  // LD r, n (00 rrr 110) — only valid in the 0x00-0x3F (00-prefix) range
  if (B0 < 0x40 && (B0 & 0x07) == 0x06) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2;
    uint8_t Reg = (B0 >> 3) & 0x07;
    if (Reg == 6) {
      Instr.setOpcode(SM83::LDHLi);
      Instr.addOperand(MCOperand::createImm(Bytes[1]));
    } else {
      Instr.setOpcode(SM83::LDri);
      Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
      Instr.addOperand(MCOperand::createImm(Bytes[1]));
    }
    return Success;
  }

  // RLCA=0x07, RRCA=0x0F, RLA=0x17, RRA=0x1F
  if (B0 == 0x07) { Instr.setOpcode(SM83::RLCA); Size = 1; return Success; }
  if (B0 == 0x0F) { Instr.setOpcode(SM83::RRCA); Size = 1; return Success; }
  if (B0 == 0x17) { Instr.setOpcode(SM83::RLA);  Size = 1; return Success; }
  if (B0 == 0x1F) { Instr.setOpcode(SM83::RRA);  Size = 1; return Success; }

  // LD (nn), SP = 0x08
  if (B0 == 0x08) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3;
    Instr.setOpcode(SM83::LDnn_SP);
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8)));
    return Success;
  }

  // STOP = 0x10 [0x00]
  if (B0 == 0x10) {
    Size = 2;
    Instr.setOpcode(SM83::STOP);
    return Success;
  }

  // ADD HL, rr = 0x09 | (rr << 4)
  if ((B0 & 0xCF) == 0x09) {
    Size = 1;
    uint8_t Pair = (B0 >> 4) & 0x03;
    Instr.setOpcode(SM83::ADDHLrr);
    Instr.addOperand(MCOperand::createReg(Reg16Table[Pair]));
    return Success;
  }

  // JR e = 0x18 + pcrel8
  if (B0 == 0x18) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2;
    Instr.setOpcode(SM83::JR);
    int8_t Off = (int8_t)Bytes[1];
    Instr.addOperand(MCOperand::createImm(Address + 2 + Off));
    return Success;
  }

  // JR cc, e (0x20/0x28/0x30/0x38)
  if ((B0 & 0xE7) == 0x20 && (B0 & 0x18) != 0) {
    // Actually: 0x20, 0x28, 0x30, 0x38
  }
  if (B0 == 0x20 || B0 == 0x28 || B0 == 0x30 || B0 == 0x38) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2;
    Instr.setOpcode(SM83::JRcc);
    uint8_t CC = (B0 >> 3) & 0x03; // NZ=0,Z=1,NC=2,C=3
    // Decode: 0x20=(NZ=0), 0x28=(Z=1), 0x30=(NC=2), 0x38=(C=3)
    int8_t Off = (int8_t)Bytes[1];
    Instr.addOperand(MCOperand::createImm(CC));
    Instr.addOperand(MCOperand::createImm(Address + 2 + Off));
    return Success;
  }

  // DAA=0x27, CPL=0x2F, SCF=0x37, CCF=0x3F
  if (B0 == 0x27) { Instr.setOpcode(SM83::DAA); Size = 1; return Success; }
  if (B0 == 0x2F) { Instr.setOpcode(SM83::CPL); Size = 1; return Success; }
  if (B0 == 0x37) { Instr.setOpcode(SM83::SCF); Size = 1; return Success; }
  if (B0 == 0x3F) { Instr.setOpcode(SM83::CCF); Size = 1; return Success; }

  // --- 01 block: LD r, r' ---
  if ((B0 & 0xC0) == 0x40) {
    // 0x40-0x7F (excluding 0x76 = HALT)
    if (B0 == 0x76) { Instr.setOpcode(SM83::HALT); Size = 1; return Success; }
    Size = 1;
    uint8_t Dst = (B0 >> 3) & 0x07;
    uint8_t Src = B0 & 0x07;
    if (Src == 6) {
      // LD r, (HL)
      if (Dst == 6) { Instr.setOpcode(SM83::HALT); return Success; } // already handled
      Instr.setOpcode(SM83::LDrHL);
      Instr.addOperand(MCOperand::createReg(Reg8Table[Dst]));
    } else if (Dst == 6) {
      // LD (HL), r
      Instr.setOpcode(SM83::LDHLr);
      Instr.addOperand(MCOperand::createReg(Reg8Table[Src]));
    } else {
      // LD r, r'
      Instr.setOpcode(SM83::LDrr);
      Instr.addOperand(MCOperand::createReg(Reg8Table[Dst]));
      Instr.addOperand(MCOperand::createReg(Reg8Table[Src]));
    }
    return Success;
  }

  // --- 10 block: ALU ops ---
  if ((B0 & 0xC0) == 0x80) {
    Size = 1;
    uint8_t Op  = (B0 >> 3) & 0x07;
    uint8_t Reg = B0 & 0x07;
    static const unsigned ALURegOpc[8] = {
        SM83::ADDr, SM83::ADCr, SM83::SUBr, SM83::SBCr,
        SM83::ANDr, SM83::XORr, SM83::ORr,  SM83::CPr
    };
    static const unsigned ALUMemOpc[8] = {
        SM83::ADDm, SM83::ADCm, SM83::SUBm, SM83::SBCm,
        SM83::ANDm, SM83::XORm, SM83::ORm,  SM83::CPm
    };
    if (Reg == 6) {
      Instr.setOpcode(ALUMemOpc[Op]);
    } else {
      Instr.setOpcode(ALURegOpc[Op]);
      Instr.addOperand(MCOperand::createReg(Reg8Table[Reg]));
    }
    return Success;
  }

  // --- 11 block ---
  // RET cc (0xC0/0xC8/0xD0/0xD8)
  if ((B0 & 0xE7) == 0xC0 && (B0 & 0x18) != 0x10) {
    // Actually 0xC0, 0xC8, 0xD0, 0xD8
  }
  if (B0 == 0xC0 || B0 == 0xC8 || B0 == 0xD0 || B0 == 0xD8) {
    Size = 1;
    Instr.setOpcode(SM83::RETcc);
    uint8_t CC = (B0 >> 3) & 0x03;
    Instr.addOperand(MCOperand::createImm(CC));
    return Success;
  }

  // POP rr (0xC1/0xD1/0xE1/0xF1)
  if (B0 == 0xC1 || B0 == 0xD1 || B0 == 0xE1 || B0 == 0xF1) {
    Size = 1;
    Instr.setOpcode(SM83::POP);
    uint8_t Pair = (B0 >> 4) & 0x03;
    addPushPop(Instr, Pair);
    return Success;
  }

  // JP cc, nn (0xC2/0xCA/0xD2/0xDA)
  if (B0 == 0xC2 || B0 == 0xCA || B0 == 0xD2 || B0 == 0xDA) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3;
    Instr.setOpcode(SM83::JPcc);
    uint8_t CC = (B0 >> 3) & 0x03;
    Instr.addOperand(MCOperand::createImm(CC));
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8)));
    return Success;
  }

  // JP nn = 0xC3
  if (B0 == 0xC3) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3;
    Instr.setOpcode(SM83::JP);
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8)));
    return Success;
  }

  // CALL cc, nn (0xC4/0xCC/0xD4/0xDC)
  if (B0 == 0xC4 || B0 == 0xCC || B0 == 0xD4 || B0 == 0xDC) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3;
    Instr.setOpcode(SM83::CALLcc);
    uint8_t CC = (B0 >> 3) & 0x03;
    Instr.addOperand(MCOperand::createImm(CC));
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8)));
    return Success;
  }

  // PUSH rr (0xC5/0xD5/0xE5/0xF5)
  if (B0 == 0xC5 || B0 == 0xD5 || B0 == 0xE5 || B0 == 0xF5) {
    Size = 1;
    Instr.setOpcode(SM83::PUSH);
    uint8_t Pair = (B0 >> 4) & 0x03;
    addPushPop(Instr, Pair);
    return Success;
  }

  // ADD/ADC/SUB/SBC/AND/XOR/OR/CP A, imm8
  if (B0 == 0xC6) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::ADDi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xCE) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::ADCi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xD6) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::SUBi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xDE) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::SBCi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xE6) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::ANDi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xEE) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::XORi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xF6) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::ORi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  if (B0 == 0xFE) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::CPi);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }

  // RST n (0xC7/0xCF/0xD7/0xDF/0xE7/0xEF/0xF7/0xFF)
  if ((B0 & 0xC7) == 0xC7) {
    Size = 1;
    Instr.setOpcode(SM83::RST);
    Instr.addOperand(MCOperand::createImm(B0 & 0x38));
    return Success;
  }

  // RET = 0xC9
  if (B0 == 0xC9) { Instr.setOpcode(SM83::RET); Size = 1; return Success; }

  // RETI = 0xD9
  if (B0 == 0xD9) { Instr.setOpcode(SM83::RETI); Size = 1; return Success; }

  // CALL nn = 0xCD
  if (B0 == 0xCD) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3;
    Instr.setOpcode(SM83::CALL);
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8)));
    return Success;
  }

  // JP HL = 0xE9
  if (B0 == 0xE9) { Instr.setOpcode(SM83::JPHL); Size = 1; return Success; }

  // LD SP, HL = 0xF9
  if (B0 == 0xF9) { Instr.setOpcode(SM83::LDSPHL); Size = 1; return Success; }

  // DI = 0xF3, EI = 0xFB
  if (B0 == 0xF3) { Instr.setOpcode(SM83::DI); Size = 1; return Success; }
  if (B0 == 0xFB) { Instr.setOpcode(SM83::EI); Size = 1; return Success; }

  // LDH (n), A = 0xE0 n
  if (B0 == 0xE0) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::LDH_nA);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  // LDH A, (n) = 0xF0 n
  if (B0 == 0xF0) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::LDH_An);
    Instr.addOperand(MCOperand::createImm(Bytes[1])); return Success;
  }
  // LDH (C), A = 0xE2
  if (B0 == 0xE2) { Instr.setOpcode(SM83::LDH_CA); Size = 1; return Success; }
  // LDH A, (C) = 0xF2
  if (B0 == 0xF2) { Instr.setOpcode(SM83::LDH_AC); Size = 1; return Success; }

  // LD (nn), A = 0xEA nn
  if (B0 == 0xEA) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3; Instr.setOpcode(SM83::LDnn_A);
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8))); return Success;
  }
  // LD A, (nn) = 0xFA nn
  if (B0 == 0xFA) {
    if (Bytes.size() < 3) { Size = 1; return Fail; }
    Size = 3; Instr.setOpcode(SM83::LDA_nn);
    Instr.addOperand(MCOperand::createImm(Bytes[1] | (Bytes[2] << 8))); return Success;
  }

  // ADD SP, n = 0xE8 n
  if (B0 == 0xE8) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::ADDSPi);
    Instr.addOperand(MCOperand::createImm((int8_t)Bytes[1])); return Success;
  }
  // LD HL, SP+n = 0xF8 n
  if (B0 == 0xF8) {
    if (Bytes.size() < 2) { Size = 1; return Fail; }
    Size = 2; Instr.setOpcode(SM83::LDHLSP);
    Instr.addOperand(MCOperand::createImm((int8_t)Bytes[1])); return Success;
  }

  // Unknown instruction
  Size = 1;
  return Fail;
}
