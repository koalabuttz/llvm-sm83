//===-- SM83InstrInfo.h - SM83 Instruction Information -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SM83_INSTR_INFO_H
#define LLVM_SM83_INSTR_INFO_H

#include "llvm/CodeGen/TargetInstrInfo.h"

#include "SM83RegisterInfo.h"

#define GET_INSTRINFO_HEADER
#include "SM83GenInstrInfo.inc"
#undef GET_INSTRINFO_HEADER

namespace llvm {

class SM83Subtarget;

class SM83InstrInfo : public SM83GenInstrInfo {
public:
  explicit SM83InstrInfo(const SM83Subtarget &STI);

  const SM83RegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
      bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
      int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      unsigned SubReg = 0,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

private:
  const SM83RegisterInfo RI;
  const SM83Subtarget &STI;
};

} // end namespace llvm

#endif // LLVM_SM83_INSTR_INFO_H
