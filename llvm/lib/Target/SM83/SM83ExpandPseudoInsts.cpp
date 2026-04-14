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

  bool expandALU8rr(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                    unsigned ALUOpc);
  bool expandShift8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                    unsigned ShiftOpc);
  bool expandCMP8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandBRCOND(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandADD16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSUB16(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandLOAD_STACK8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
  bool expandSTORE_STACK8(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
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
  case SM83::SHL8r:  return expandShift8(MBB, MI, SM83::SLAr);
  case SM83::SRL8r:  return expandShift8(MBB, MI, SM83::SRLr);
  case SM83::SRA8r:  return expandShift8(MBB, MI, SM83::SRAr);
  case SM83::CMP8rr: return expandCMP8(MBB, MI);
  case SM83::BRCONDcc: return expandBRCOND(MBB, MI);
  case SM83::LOAD8: return expandLOAD8(MBB, MI);
  case SM83::STORE8: return expandSTORE8(MBB, MI);
  case SM83::ADD16rr: return expandADD16(MBB, MI);
  case SM83::SUB16rr: return expandSUB16(MBB, MI);
  case SM83::LOAD_STACK8: return expandLOAD_STACK8(MBB, MI);
  case SM83::STORE_STACK8: return expandSTORE_STACK8(MBB, MI);
  default:
    return false;
  }
}

bool SM83ExpandPseudo::expandALU8rr(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI,
                                    unsigned ALUOpc) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register Src1Reg = MI->getOperand(1).getReg();
  Register Src2Reg = MI->getOperand(2).getReg();

  // LD A, src1
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::A)
      .addReg(Src1Reg);
  // ALU A, src2
  BuildMI(MBB, MI, DL, TII->get(ALUOpc))
      .addReg(Src2Reg);
  // LD dst, A
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstReg)
      .addReg(SM83::A);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandShift8(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI,
                                    unsigned ShiftOpc) {
  // For simplicity, shift by 1 and loop. This is a placeholder —
  // a proper implementation would handle known shift amounts.
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  Register SrcReg = MI->getOperand(1).getReg();

  // Copy src to dst, then shift dst by 1.
  // TODO: handle variable shift amounts with a loop.
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstReg)
      .addReg(SrcReg);
  BuildMI(MBB, MI, DL, TII->get(ShiftOpc), DstReg)
      .addReg(DstReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandCMP8(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register LHSReg = MI->getOperand(0).getReg();
  Register RHSReg = MI->getOperand(1).getReg();

  // LD A, lhs
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::A)
      .addReg(LHSReg);
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

bool SM83ExpandPseudo::expandSUB16(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI) {
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

  // Sub low bytes: LD A, src1.lo / SUB src2.lo / LD dst.lo, A
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::A).addReg(Src1Lo);
  BuildMI(MBB, MI, DL, TII->get(SM83::SUBr)).addReg(Src2Lo);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstLo).addReg(SM83::A);
  // Sub high bytes with carry: LD A, src1.hi / SBC A, src2.hi / LD dst.hi, A
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), SM83::A).addReg(Src1Hi);
  BuildMI(MBB, MI, DL, TII->get(SM83::SBCr)).addReg(Src2Hi);
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrr), DstHi).addReg(SM83::A);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandLOAD_STACK8(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register DstReg = MI->getOperand(0).getReg();
  MachineOperand &OffsetOp = MI->getOperand(1);

  // LD HL, SP + offset
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLSP))
      .add(OffsetOp);
  // LD dst, (HL)
  BuildMI(MBB, MI, DL, TII->get(SM83::LDrHL), DstReg);

  MI->eraseFromParent();
  return true;
}

bool SM83ExpandPseudo::expandSTORE_STACK8(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MI) {
  DebugLoc DL = MI->getDebugLoc();
  Register SrcReg = MI->getOperand(0).getReg();
  MachineOperand &OffsetOp = MI->getOperand(1);

  // LD HL, SP + offset
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLSP))
      .add(OffsetOp);
  // LD (HL), src
  BuildMI(MBB, MI, DL, TII->get(SM83::LDHLr))
      .addReg(SrcReg);

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

FunctionPass *llvm::createSM83ExpandPseudoPass() {
  return new SM83ExpandPseudo();
}
