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
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

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

  // 16-bit register pair copy: handles GR16 (BC, DE, HL) and subsets.
  // Exclude AF and SP — AF contains the non-copyable F register,
  // SP requires LDSPHL/LDHLSP and is handled below.
  if (SM83::GR16RegClass.contains(DestReg) &&
      SM83::GR16RegClass.contains(SrcReg)) {
    Register DestLo = RI.getSubReg(DestReg, SM83::sub_lo);
    Register DestHi = RI.getSubReg(DestReg, SM83::sub_hi);
    Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
    Register SrcHi = RI.getSubReg(SrcReg, SM83::sub_hi);

    if (DestLo && DestHi && SrcLo && SrcHi) {
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
  }

  // SP → HL: LD HL, SP+0 (read stack pointer into HL).
  if (DestReg == SM83::HL && SrcReg == SM83::SP) {
    BuildMI(MBB, MI, DL, get(SM83::LDHLSP)).addImm(0);
    return;
  }

  // HL → SP: LD SP, HL.
  if (DestReg == SM83::SP && SrcReg == SM83::HL) {
    BuildMI(MBB, MI, DL, get(SM83::LDSPHL));
    return;
  }

  // i8 → i16 pair copy (e.g., A → BC): zero-extend by copying to lo, clearing hi.
  if (SM83::GR16RegClass.contains(DestReg) &&
      SM83::GPR8RegClass.contains(SrcReg)) {
    Register DestLo = RI.getSubReg(DestReg, SM83::sub_lo);
    Register DestHi = RI.getSubReg(DestReg, SM83::sub_hi);
    BuildMI(MBB, MI, DL, get(SM83::LDrr), DestLo)
        .addReg(SrcReg, getKillRegState(KillSrc));
    BuildMI(MBB, MI, DL, get(SM83::LDri), DestHi).addImm(0);
    return;
  }

  // Provide detailed error for debugging.
  errs() << "SM83 copyPhysReg failure: dest=" << printReg(DestReg, &RI)
         << " src=" << printReg(SrcReg, &RI) << "\n";
  errs() << "  dest in GPR8=" << SM83::GPR8RegClass.contains(DestReg)
         << " GR16=" << SM83::GR16RegClass.contains(DestReg)
         << " GPR16=" << SM83::GPR16RegClass.contains(DestReg) << "\n";
  errs() << "  src in GPR8=" << SM83::GPR8RegClass.contains(SrcReg)
         << " GR16=" << SM83::GR16RegClass.contains(SrcReg)
         << " GPR16=" << SM83::GPR16RegClass.contains(SrcReg) << "\n";
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
  } else if (SM83::GR16RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(SM83::STORE_FI16))
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
  } else if (SM83::GR16RegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MI, DL, get(SM83::LOAD_FI16), DestReg)
        .addFrameIndex(FrameIndex);
  } else {
    report_fatal_error("SM83: unsupported register class for stack reload");
  }
}

} // end namespace llvm
