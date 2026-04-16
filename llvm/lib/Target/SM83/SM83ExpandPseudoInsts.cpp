//===-- SM83ExpandPseudoInsts.cpp - Expand SM83 pseudo instructions -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass expands SM83 pseudo instructions into sequences of real SM83
// instructions. This is needed primarily because the SM83 ALU operates
// through the accumulator register A, but the register allocator works
// with arbitrary register operands.
//
//===----------------------------------------------------------------------===//

#include "SM83.h"
#include "SM83InstrInfo.h"
#include "SM83Subtarget.h"
#include "SM83TargetMachine.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "sm83-expand-pseudo"
#define PASS_NAME "SM83 pseudo instruction expansion"

namespace {

class SM83ExpandPseudo : public MachineFunctionPass {
public:
  static char ID;
  SM83ExpandPseudo() : MachineFunctionPass(ID) {
    initializeSM83ExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const SM83InstrInfo *TII = nullptr;

  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);

  void emitLD(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
              const DebugLoc &DL, Register DstReg, Register SrcReg);
  bool expandALU8rr(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                    unsigned ALUOpc);
  bool expandCMP8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandBRCOND(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandADD16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandBitwise16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                       unsigned LoOpc, unsigned HiOpc);
  bool expandLOAD16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD_STACK8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE_STACK8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSEXT8_16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandCMPC8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD_PTR8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE_PTR8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD_BCDE_A(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE_BCDE_A(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD_PTR16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE_PTR16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD_FI16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE_FI16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandCMP16rr(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandCMPC16rr(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandCMP16EQrr(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLDH_LOAD8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLDH_STORE8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
};

} // namespace

char SM83ExpandPseudo::ID = 0;

INITIALIZE_PASS(SM83ExpandPseudo, DEBUG_TYPE, PASS_NAME, false, false)

bool SM83ExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<SM83Subtarget>().getInstrInfo();
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool SM83ExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (auto MI = MBB.begin(), E = MBB.end(); MI != E;) {
    auto Next = std::next(MI);
    Modified |= expandMI(MBB, MI);
    MI = Next;
  }
  return Modified;
}

bool SM83ExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) {
  switch (MI->getOpcode()) {
  case SM83::ADD8rr: return expandALU8rr(MBB, MI, SM83::ADDr);
  case SM83::SUB8rr: return expandALU8rr(MBB, MI, SM83::SUBr);
  case SM83::AND8rr: return expandALU8rr(MBB, MI, SM83::ANDr);
  case SM83::OR8rr:  return expandALU8rr(MBB, MI, SM83::ORr);
  case SM83::XOR8rr: return expandALU8rr(MBB, MI, SM83::XORr);
  case SM83::CMP8rr: return expandCMP8(MBB, MI);
  case SM83::BRCONDcc: return expandBRCOND(MBB, MI);
  case SM83::LOAD8: return expandLOAD8(MBB, MI);
  case SM83::STORE8: return expandSTORE8(MBB, MI);
  case SM83::LOAD16: return expandLOAD16(MBB, MI);
  case SM83::STORE16: return expandSTORE16(MBB, MI);
  case SM83::ADD16rr: return expandADD16(MBB, MI);
  case SM83::SUB16rr: return expandBitwise16(MBB, MI, SM83::SUBr, SM83::SBCr);
  case SM83::AND16rr: return expandBitwise16(MBB, MI, SM83::ANDr, SM83::ANDr);
  case SM83::OR16rr:  return expandBitwise16(MBB, MI, SM83::ORr, SM83::ORr);
  case SM83::XOR16rr: return expandBitwise16(MBB, MI, SM83::XORr, SM83::XORr);
  case SM83::LOAD_STACK8: return expandLOAD_STACK8(MBB, MI);
  case SM83::STORE_STACK8: return expandSTORE_STACK8(MBB, MI);
  case SM83::LOAD_FI8: return expandLOAD_STACK8(MBB, MI);
  case SM83::STORE_FI8: return expandSTORE_STACK8(MBB, MI);
  case SM83::LOAD_FI16: return expandLOAD_FI16(MBB, MI);
  case SM83::STORE_FI16: return expandSTORE_FI16(MBB, MI);
  case SM83::LOAD_PTR8: return expandLOAD_PTR8(MBB, MI);
  case SM83::STORE_PTR8: return expandSTORE_PTR8(MBB, MI);
  case SM83::LOAD_BCDE_A: return expandLOAD_BCDE_A(MBB, MI);
  case SM83::STORE_BCDE_A: return expandSTORE_BCDE_A(MBB, MI);
  case SM83::LOAD_PTR16: return expandLOAD_PTR16(MBB, MI);
  case SM83::STORE_PTR16: return expandSTORE_PTR16(MBB, MI);
  case SM83::SEXT8_16: return expandSEXT8_16(MBB, MI);
  case SM83::CMPC8rr: return expandCMPC8(MBB, MI);
  case SM83::CMP16rr: return expandCMP16rr(MBB, MI);
  case SM83::CMPC16rr: return expandCMPC16rr(MBB, MI);
  case SM83::CMP16EQrr: return expandCMP16EQrr(MBB, MI);
  case SM83::LDH_LOAD8: return expandLDH_LOAD8(MBB, MI);
  case SM83::LDH_STORE8: return expandLDH_STORE8(MBB, MI);
  default:
    return false;
  }
}

// Emit LD dst, src only if they differ (eliminates identity loads).
void SM83ExpandPseudo::emitLD(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MI,
                              const DebugLoc &DL,
                              Register DstReg, Register SrcReg) {
  if (DstReg != SrcReg)
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstReg).addReg(SrcReg);
}

bool SM83ExpandPseudo::expandALU8rr(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI,
                                    unsigned ALUOpc) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register Src1Reg = MI->getOperand(1).getReg();
  Register Src2Reg = MI->getOperand(2).getReg();

  // LD A, src1 (skip if already A)
  emitLD(MBB, MI, DL, SM83::A, Src1Reg);
  // ALU A, src2
  BuildMI(MBB, MI, DL, TII->get(ALUOpc))
      .addReg(Src2Reg);
  // LD dst, A (skip if already A)
  emitLD(MBB, MI, DL, DstReg, SM83::A);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandCMP8(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register LHSReg = MI->getOperand(0).getReg();
  Register RHSReg = MI->getOperand(1).getReg();

  // LD A, lhs (skip if already A)
  emitLD(MBB, MI, DL, SM83::A, LHSReg);
  // CP rhs (compares A with rhs, sets flags)
  BuildMI(MBB, MI, DL, TII->get(SM83::CPr))
      .addReg(RHSReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandBRCOND(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  MachineOperand &Target = MI->getOperand(0);
  unsigned CC = MI->getOperand(1).getImm();

  // Emit JR cc, target.
  BuildMI(MBB, MI, DL, TII->get(SM83::JRcc))
      .addImm(CC)
      .add(Target);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandADD16(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register Src1Reg = MI->getOperand(1).getReg();
  Register Src2Reg = MI->getOperand(2).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();

  // Copy src1 to HL.
  Register Src1Lo = RI.getSubReg(Src1Reg, SM83::sub_lo);
  Register Src1Hi = RI.getSubReg(Src1Reg, SM83::sub_hi);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::L).addReg(Src1Lo);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::H).addReg(Src1Hi);
  // ADD HL, src2
  BuildMI(MBB, MI, DL, TII->get(SM83::ADDHLrr)).addReg(Src2Reg);
  // Copy HL to dst.
  Register DstLo = RI.getSubReg(DstReg, SM83::sub_lo);
  Register DstHi = RI.getSubReg(DstReg, SM83::sub_hi);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstLo).addReg(SM83::L);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstHi).addReg(SM83::H);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandBitwise16(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI,
                                       unsigned LoOpc, unsigned HiOpc) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register Src1Reg = MI->getOperand(1).getReg();
  Register Src2Reg = MI->getOperand(2).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();

  Register Src1Lo = RI.getSubReg(Src1Reg, SM83::sub_lo);
  Register Src1Hi = RI.getSubReg(Src1Reg, SM83::sub_hi);
  Register Src2Lo = RI.getSubReg(Src2Reg, SM83::sub_lo);
  Register Src2Hi = RI.getSubReg(Src2Reg, SM83::sub_hi);
  Register DstLo = RI.getSubReg(DstReg, SM83::sub_lo);
  Register DstHi = RI.getSubReg(DstReg, SM83::sub_hi);

  // Low bytes: LD A, src1.lo / OP src2.lo / LD dst.lo, A
  emitLD(MBB, MI, DL, SM83::A, Src1Lo);
  BuildMI(MBB, MI, DL, TII->get(LoOpc)).addReg(Src2Lo);
  emitLD(MBB, MI, DL, DstLo, SM83::A);
  // High bytes: LD A, src1.hi / OP src2.hi / LD dst.hi, A
  emitLD(MBB, MI, DL, SM83::A, Src1Hi);
  BuildMI(MBB, MI, DL, TII->get(HiOpc)).addReg(Src2Hi);
  emitLD(MBB, MI, DL, DstHi, SM83::A);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD16(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  MachineOperand &Addr = MI->getOperand(1);
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register DstLo = RI.getSubReg(DstReg, SM83::sub_lo);
  Register DstHi = RI.getSubReg(DstReg, SM83::sub_hi);

  // LD HL, addr / LD lo, (HL+) / LD hi, (HL)
  // Use HL+ (auto-increment) for efficiency.
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrri), SM83::HL).add(Addr);
  // Load low byte via HL+, then high byte.
  // SM83 LD A,(HL+) auto-increments HL — but only A can do this.
  // So: LD A,(HL+) / LD dst.lo,A / LD dst.hi,(HL)
  BuildMI(MBB, MI, DL, TII->get(SM83::LDA_HLI));
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstLo).addReg(SM83::A);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE16(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  MachineOperand &Addr = MI->getOperand(1);
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
  Register SrcHi = RI.getSubReg(SrcReg, SM83::sub_hi);

  // LD HL, addr / LD A, src.lo / LD (HL+),A / LD (HL), src.hi
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrri), SM83::HL).add(Addr);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::A).addReg(SrcLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLI_A));
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr)).addReg(SrcHi);

  MI->eraseFromParent();
  return true;
}

// Emit either LDHLSP (offset fits in s8) or LDrri+ADDHLrr (large frame).
static void emitLoadHLSP(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                         DebugLoc DL, const SM83InstrInfo *TII, int64_t Offset) {
  if (Offset >= -128 && Offset <= 127) {
    BuildMI(MBB, MI, DL, TII->get(SM83::LDHLSP)).addImm(Offset);
  } else {
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrri), SM83::HL).addImm(Offset);
    BuildMI(MBB, MI, DL, TII->get(SM83::ADDHLrr)).addReg(SM83::SP);
  }
}

bool SM83ExpandPseudo::expandLOAD_STACK8(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  int64_t Offset = MI->getOperand(1).getImm();

  emitLoadHLSP(MBB, MI, DL, TII, Offset);
  // LD dst, (HL)
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE_STACK8(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  int64_t Offset = MI->getOperand(1).getImm();

  emitLoadHLSP(MBB, MI, DL, TII, Offset);
  // LD (HL), src
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr)).addReg(SrcReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD8(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  MachineOperand &Addr = MI->getOperand(1);

  // LD HL, addr
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrri), SM83::HL)
      .add(Addr);
  // LD dst, (HL)
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE8(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  MachineOperand &Addr = MI->getOperand(1);

  // LD HL, addr
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrri), SM83::HL)
      .add(Addr);
  // LD (HL), src
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr))
      .addReg(SrcReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSEXT8_16(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register SrcReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register DstLo = RI.getSubReg(DstReg, SM83::sub_lo);
  Register DstHi = RI.getSubReg(DstReg, SM83::sub_hi);

  // Copy src to lo byte.
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstLo).addReg(SrcReg);
  // Compute sign byte: LD A, src; ADD A, A (shifts bit 7 to carry); SBC A, A
  // Result: 0x00 if bit 7 was 0 (positive), 0xFF if bit 7 was 1 (negative).
  emitLD(MBB, MI, DL, SM83::A, SrcReg);
  BuildMI(MBB, MI, DL, TII->get(SM83::ADDr)).addReg(SM83::A); // ADD A, A
  BuildMI(MBB, MI, DL, TII->get(SM83::SBCr)).addReg(SM83::A); // SBC A, A
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstHi).addReg(SM83::A);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandCMPC8(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register LHSReg = MI->getOperand(0).getReg();
  Register RHSReg = MI->getOperand(1).getReg();

  // LD A, lhs; SBC A, rhs — subtracts rhs and carry from A, sets flags.
  // Used as the hi-byte step after CMP8rr on lo bytes for 16-bit comparison.
  emitLD(MBB, MI, DL, SM83::A, LHSReg);
  BuildMI(MBB, MI, DL, TII->get(SM83::SBCr)).addReg(RHSReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD_PTR8(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register PtrReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();

  // Fast path: load to A via BC or DE — no HL copy needed.
  if (DstReg == SM83::A) {
    if (PtrReg == SM83::BC) {
      BuildMI(MBB, MI, DL, TII->get(SM83::LDA_BC));
      MI->eraseFromParent();
      return true;
    }
    if (PtrReg == SM83::DE) {
      BuildMI(MBB, MI, DL, TII->get(SM83::LDA_DE));
      MI->eraseFromParent();
      return true;
    }
  }

  // Copy ptr to HL if not already there.
  if (PtrReg != SM83::HL) {
    Register PtrLo = RI.getSubReg(PtrReg, SM83::sub_lo);
    Register PtrHi = RI.getSubReg(PtrReg, SM83::sub_hi);
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::L).addReg(PtrLo);
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::H).addReg(PtrHi);
  }
  // LD dst, (HL)
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE_PTR8(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  Register PtrReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();

  // Fast path: store from A via BC or DE — no HL copy needed.
  if (SrcReg == SM83::A) {
    if (PtrReg == SM83::BC) {
      BuildMI(MBB, MI, DL, TII->get(SM83::LDBC_A));
      MI->eraseFromParent();
      return true;
    }
    if (PtrReg == SM83::DE) {
      BuildMI(MBB, MI, DL, TII->get(SM83::LDDE_A));
      MI->eraseFromParent();
      return true;
    }
  }

  // Copy ptr to HL if not already there.
  if (PtrReg != SM83::HL) {
    Register PtrLo = RI.getSubReg(PtrReg, SM83::sub_lo);
    Register PtrHi = RI.getSubReg(PtrReg, SM83::sub_hi);
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::L).addReg(PtrLo);
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::H).addReg(PtrHi);
  }
  // LD (HL), src
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr)).addReg(SrcReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD_BCDE_A(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register PtrReg = MI->getOperand(1).getReg();

  if (PtrReg == SM83::BC)
    BuildMI(MBB, MI, DL, TII->get(SM83::LDA_BC));
  else if (PtrReg == SM83::DE)
    BuildMI(MBB, MI, DL, TII->get(SM83::LDA_DE));
  else
    llvm_unreachable("LOAD_BCDE_A: pointer must be BC or DE");

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE_BCDE_A(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register PtrReg = MI->getOperand(1).getReg();

  if (PtrReg == SM83::BC)
    BuildMI(MBB, MI, DL, TII->get(SM83::LDBC_A));
  else if (PtrReg == SM83::DE)
    BuildMI(MBB, MI, DL, TII->get(SM83::LDDE_A));
  else
    llvm_unreachable("STORE_BCDE_A: pointer must be BC or DE");

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD_PTR16(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register PtrReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register DstLo = RI.getSubReg(DstReg, SM83::sub_lo);
  Register DstHi = RI.getSubReg(DstReg, SM83::sub_hi);
  assert(DstLo && DstHi && "DST reg missing sub_lo/sub_hi in LOAD_PTR16 expansion");

  // Copy ptr to HL if not already there.
  if (PtrReg != SM83::HL) {
    Register PtrLo = RI.getSubReg(PtrReg, SM83::sub_lo);
    Register PtrHi = RI.getSubReg(PtrReg, SM83::sub_hi);
    assert(PtrLo && PtrHi && "PTR reg missing sub_lo/sub_hi in LOAD_PTR16 expansion");
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::L).addReg(PtrLo);
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::H).addReg(PtrHi);
  }
  // LD A,(HL+); LD dst.lo,A; LD dst.hi,(HL)
  BuildMI(MBB, MI, DL, TII->get(SM83::LDA_HLI));
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstLo).addReg(SM83::A);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE_PTR16(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  Register PtrReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
  Register SrcHi = RI.getSubReg(SrcReg, SM83::sub_hi);
  assert(SrcLo && SrcHi && "SRC reg missing sub_lo/sub_hi in STORE_PTR16 expansion");

  // Copy ptr to HL if not already there.
  if (PtrReg != SM83::HL) {
    Register PtrLo = RI.getSubReg(PtrReg, SM83::sub_lo);
    Register PtrHi = RI.getSubReg(PtrReg, SM83::sub_hi);
    assert(PtrLo && PtrHi && "PTR reg missing sub_lo/sub_hi in STORE_PTR16 expansion");
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::L).addReg(PtrLo);
    BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::H).addReg(PtrHi);
  }
  // LD A,src.lo; LD (HL+),A; LD (HL),src.hi
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::A).addReg(SrcLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLI_A));
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr)).addReg(SrcHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD_FI16(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  MachineOperand &FI = MI->getOperand(1);
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register DstLo = RI.getSubReg(DstReg, SM83::sub_lo);
  Register DstHi = RI.getSubReg(DstReg, SM83::sub_hi);
  assert(DstLo && DstHi && "DST reg missing sub_lo/sub_hi in LOAD_FI16 expansion");

  // LD HL, SP + offset; LD lo, (HL); INC HL; LD hi, (HL)
  emitLoadHLSP(MBB, MI, DL, TII, FI.getImm());
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::INCrr), SM83::HL).addReg(SM83::HL);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE_FI16(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  MachineOperand &FI = MI->getOperand(1);
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
  Register SrcHi = RI.getSubReg(SrcReg, SM83::sub_hi);
  assert(SrcLo && SrcHi && "SRC reg missing sub_lo/sub_hi in STORE_FI16 expansion");

  // LD HL, SP + offset; LD (HL), lo; INC HL; LD (HL), hi
  emitLoadHLSP(MBB, MI, DL, TII, FI.getImm());
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr)).addReg(SrcLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::INCrr), SM83::HL).addReg(SM83::HL);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr)).addReg(SrcHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandCMP16rr(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register LHSReg = MI->getOperand(0).getReg();
  Register RHSReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register LHSLo = RI.getSubReg(LHSReg, SM83::sub_lo);
  Register LHSHi = RI.getSubReg(LHSReg, SM83::sub_hi);
  Register RHSLo = RI.getSubReg(RHSReg, SM83::sub_lo);
  Register RHSHi = RI.getSubReg(RHSReg, SM83::sub_hi);

  // LD A, lhs.lo; CP rhs.lo  → C = (lhs.lo < rhs.lo unsigned)
  emitLD(MBB, MI, DL, SM83::A, LHSLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::CPr)).addReg(RHSLo);
  // LD A, lhs.hi; SBC A, rhs.hi → C = borrow = (lhs < rhs unsigned)
  emitLD(MBB, MI, DL, SM83::A, LHSHi);
  BuildMI(MBB, MI, DL, TII->get(SM83::SBCr)).addReg(RHSHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandCMP16EQrr(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register LHSReg = MI->getOperand(0).getReg();
  Register RHSReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register LHSLo = RI.getSubReg(LHSReg, SM83::sub_lo);
  Register LHSHi = RI.getSubReg(LHSReg, SM83::sub_hi);
  Register RHSLo = RI.getSubReg(RHSReg, SM83::sub_lo);
  Register RHSHi = RI.getSubReg(RHSReg, SM83::sub_hi);

  // LD A, lhs.lo; XOR rhs.lo; LD L, A   — save lo_diff in L (declared clobbered)
  emitLD(MBB, MI, DL, SM83::A, LHSLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::XORr)).addReg(RHSLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::L).addReg(SM83::A);
  // LD A, lhs.hi; XOR rhs.hi; OR L      — A = lo_diff | hi_diff; Z=1 iff lhs==rhs
  emitLD(MBB, MI, DL, SM83::A, LHSHi);
  BuildMI(MBB, MI, DL, TII->get(SM83::XORr)).addReg(RHSHi);
  BuildMI(MBB, MI, DL, TII->get(SM83::ORr)).addReg(SM83::L);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandCMPC16rr(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register LHSReg = MI->getOperand(0).getReg();
  Register RHSReg = MI->getOperand(1).getReg();
  const SM83RegisterInfo &RI = TII->getRegisterInfo();
  Register LHSLo = RI.getSubReg(LHSReg, SM83::sub_lo);
  Register LHSHi = RI.getSubReg(LHSReg, SM83::sub_hi);
  Register RHSLo = RI.getSubReg(RHSReg, SM83::sub_lo);
  Register RHSHi = RI.getSubReg(RHSReg, SM83::sub_hi);
  assert(LHSLo && LHSHi && "LHS reg missing sub_lo/sub_hi in CMPC16rr expansion");
  assert(RHSLo && RHSHi && "RHS reg missing sub_lo/sub_hi in CMPC16rr expansion");

  // LD A, lhs.lo; SBC A, rhs.lo (uses carry from previous compare in chain)
  emitLD(MBB, MI, DL, SM83::A, LHSLo);
  BuildMI(MBB, MI, DL, TII->get(SM83::SBCr)).addReg(RHSLo);
  // LD A, lhs.hi; SBC A, rhs.hi
  emitLD(MBB, MI, DL, SM83::A, LHSHi);
  BuildMI(MBB, MI, DL, TII->get(SM83::SBCr)).addReg(RHSHi);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLDH_LOAD8(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  MachineOperand &AddrOp = MI->getOperand(1);

  // LDH A, [n]
  BuildMI(MBB, MI, DL, TII->get(SM83::LDH_An)).add(AddrOp);
  // LD dst, A (skip if dst is already A)
  emitLD(MBB, MI, DL, DstReg, SM83::A);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLDH_STORE8(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  MachineOperand &AddrOp = MI->getOperand(1);

  // LD A, src (skip if src is already A)
  emitLD(MBB, MI, DL, SM83::A, SrcReg);
  // LDH [n], A
  BuildMI(MBB, MI, DL, TII->get(SM83::LDH_nA)).add(AddrOp);

  MI->eraseFromParent();
  return true;
}

FunctionPass *llvm::createSM83ExpandPseudoPass() {
  return new SM83ExpandPseudo();
}
