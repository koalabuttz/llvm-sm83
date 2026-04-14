//===-- SM83FrameLowering.cpp - SM83 Frame Information ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83FrameLowering.h"

#include "SM83.h"
#include "SM83InstrInfo.h"
#include "SM83Subtarget.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

namespace llvm {

SM83FrameLowering::SM83FrameLowering()
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                          /*StackAlignment=*/Align(2),
                          /*LocalAreaOffset=*/0) {}

void SM83FrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const SM83Subtarget &STI = MF.getSubtarget<SM83Subtarget>();
  const SM83InstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  uint64_t StackSize = MFI.getStackSize();
  if (StackSize == 0)
    return;

  // SM83 can adjust SP by a signed 8-bit immediate with ADD SP, imm8s.
  // For larger frames, emit multiple ADD SP instructions.
  int64_t Remaining = -(int64_t)StackSize;
  while (Remaining != 0) {
    int64_t Adj = std::max(Remaining, (int64_t)-128);
    Adj = std::min(Adj, (int64_t)127);
    if (Adj == 0)
      break;
    BuildMI(MBB, MBBI, DL, TII.get(SM83::ADDSPi))
        .addImm(Adj);
    Remaining -= Adj;
  }
}

void SM83FrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const SM83Subtarget &STI = MF.getSubtarget<SM83Subtarget>();
  const SM83InstrInfo &TII = *STI.getInstrInfo();
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  uint64_t StackSize = MFI.getStackSize();
  if (StackSize == 0)
    return;

  int64_t Remaining = (int64_t)StackSize;
  while (Remaining != 0) {
    int64_t Adj = std::min(Remaining, (int64_t)127);
    if (Adj == 0)
      break;
    BuildMI(MBB, MBBI, DL, TII.get(SM83::ADDSPi))
        .addImm(Adj);
    Remaining -= Adj;
  }
}

bool SM83FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}

MachineBasicBlock::iterator SM83FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  const SM83Subtarget &STI = MF.getSubtarget<SM83Subtarget>();
  const SM83InstrInfo &TII = *STI.getInstrInfo();

  if (!hasReservedCallFrame(MF)) {
    int64_t Amount = MI->getOperand(0).getImm();
    if (Amount != 0) {
      if (MI->getOpcode() == SM83::ADJCALLSTACKDOWN)
        Amount = -Amount;
      // Emit ADD SP, amount.
      BuildMI(MBB, MI, MI->getDebugLoc(), TII.get(SM83::ADDSPi))
          .addImm(Amount);
    }
  }
  return MBB.erase(MI);
}

bool SM83FrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

} // end namespace llvm
