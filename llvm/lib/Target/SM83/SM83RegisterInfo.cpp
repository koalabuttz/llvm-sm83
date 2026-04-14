//===-- SM83RegisterInfo.cpp - SM83 Register Information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83RegisterInfo.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

#include "SM83.h"
#include "SM83InstrInfo.h"
#include "SM83TargetMachine.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

#define GET_REGINFO_TARGET_DESC
#include "SM83GenRegisterInfo.inc"

namespace llvm {

SM83RegisterInfo::SM83RegisterInfo() : SM83GenRegisterInfo(0) {}

const uint16_t *
SM83RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_Normal_SaveList;
}

const uint32_t *
SM83RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID CC) const {
  return CSR_Normal_RegMask;
}

BitVector SM83RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  // Reserve the stack pointer.
  Reserved.set(SM83::SPL);
  Reserved.set(SM83::SPH);
  Reserved.set(SM83::SP);

  // Reserve the flags register.
  Reserved.set(SM83::F);

  return Reserved;
}

bool SM83RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                           int SPAdj,
                                           unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  // Stub — will be implemented in Phase 4.
  report_fatal_error("SM83: frame index elimination not yet implemented");
}

Register SM83RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return SM83::SP;
}

} // end namespace llvm
