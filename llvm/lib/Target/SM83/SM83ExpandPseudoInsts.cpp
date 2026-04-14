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

FunctionPass *llvm::createSM83ExpandPseudoPass() {
  return new SM83ExpandPseudo();
}
