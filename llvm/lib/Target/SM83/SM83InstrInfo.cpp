//===-- SM83InstrInfo.cpp - SM83 Instruction Information -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83InstrInfo.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

#include "SM83.h"
#include "SM83Subtarget.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "SM83GenInstrInfo.inc"

namespace llvm {

SM83InstrInfo::SM83InstrInfo(const SM83Subtarget &STI)
    : SM83GenInstrInfo(STI, RI, SM83::ADJCALLSTACKDOWN, SM83::ADJCALLSTACKUP),
      RI(), STI(STI) {}

void SM83InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI,
                                const DebugLoc &DL, Register DestReg,
                                Register SrcReg, bool KillSrc,
                                bool RenamableDest, bool RenamableSrc) const {
  if (SM83::GPR8RegClass.contains(DestReg, SrcReg)) {
    // 8-bit register copy: LD r, r'
    BuildMI(MBB, MI, DL, get(SM83::LDrr), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  if (SM83::GR16RegClass.contains(DestReg) &&
      SM83::GR16RegClass.contains(SrcReg)) {
    // 16-bit register pair copy: copy lo and hi bytes separately.
    Register DestLo = RI.getSubReg(DestReg, SM83::sub_lo);
    Register DestHi = RI.getSubReg(DestReg, SM83::sub_hi);
    Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
    Register SrcHi = RI.getSubReg(SrcReg, SM83::sub_hi);

    // Handle overlap: if DestLo == SrcHi, do hi first.
    if (DestLo == SrcHi) {
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestHi)
          .addReg(SrcHi, getKillRegState(KillSrc) | RegState::Undef);
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestLo)
          .addReg(SrcLo, getKillRegState(KillSrc) | RegState::Undef);
    } else {
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestLo)
          .addReg(SrcLo, getKillRegState(KillSrc) | RegState::Undef);
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestHi)
          .addReg(SrcHi, getKillRegState(KillSrc) | RegState::Undef);
    }
    return;
  }

  report_fatal_error("SM83: cannot copy between these register classes");
}

void SM83InstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  if (SM83::GPR8RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(SM83::STORE_STACK8))
        .addReg(SrcReg, getKillRegState(isKill))
        .addFrameIndex(FrameIndex);
  } else {
    report_fatal_error("SM83: unsupported register class for stack spill");
  }
}

void SM83InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    unsigned SubReg, MachineInstr::MIFlag Flags) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  if (SM83::GPR8RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(SM83::LOAD_STACK8), DestReg)
        .addFrameIndex(FrameIndex);
  } else {
    report_fatal_error("SM83: unsupported register class for stack reload");
  }
}

} // end namespace llvm
