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

  // SP → GR16: first load SP into HL via LDHLSP, then copy HL to dest.
  if (SrcReg == SM83::SP && SM83::GR16RegClass.contains(DestReg)) {
    BuildMI(MBB, MI, DL, get(SM83::LDHLSP)).addImm(0);
    if (DestReg != SM83::HL) {
      Register DestLo = RI.getSubReg(DestReg, SM83::sub_lo);
      Register DestHi = RI.getSubReg(DestReg, SM83::sub_hi);
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestLo).addReg(SM83::L);
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestHi).addReg(SM83::H);
    }
    return;
  }

  // GR16 → SP: copy to HL first if needed, then LD SP, HL.
  if (DestReg == SM83::SP && SM83::GR16RegClass.contains(SrcReg)) {
    if (SrcReg != SM83::HL) {
      Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
      Register SrcHi = RI.getSubReg(SrcReg, SM83::sub_hi);
      BuildMI(MBB, MI, DL, get(SM83::LDrr), SM83::L).addReg(SrcLo);
      BuildMI(MBB, MI, DL, get(SM83::LDrr), SM83::H).addReg(SrcHi);
    }
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

  // i16 pair → i8 copy (e.g., BC → D): extract the low byte.
  if (SM83::GPR8RegClass.contains(DestReg) &&
      SM83::GR16RegClass.contains(SrcReg)) {
    Register SrcLo = RI.getSubReg(SrcReg, SM83::sub_lo);
    if (SrcLo) {
      BuildMI(MBB, MI, DL, get(SM83::LDrr), DestReg)
          .addReg(SrcLo, getKillRegState(KillSrc));
      return;
    }
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

// ==========================================================================
// Branch analysis for the Control Flow Optimizer.
// ==========================================================================

/// Return true if the instruction is an unconditional branch (JP).
static bool isUncondBranch(const MachineInstr &MI) {
  return MI.getOpcode() == SM83::JP || MI.getOpcode() == SM83::JR;
}

/// Return true if the instruction is a conditional branch (JPcc/JRcc).
static bool isCondBranch(const MachineInstr &MI) {
  return MI.getOpcode() == SM83::JPcc || MI.getOpcode() == SM83::JRcc;
}

/// Get the condition code operand index for a conditional branch.
/// For JPcc/JRcc: operand 0 = cc, operand 1 = target.
static unsigned getCondFromBranch(const MachineInstr &MI) {
  assert(isCondBranch(MI));
  return MI.getOperand(0).getImm();
}

/// Get the branch target operand.
static MachineBasicBlock *getBranchTarget(const MachineInstr &MI) {
  if (isCondBranch(MI))
    return MI.getOperand(1).getMBB();
  return MI.getOperand(0).getMBB();
}

/// SM83 condition codes: 0=NZ, 1=Z, 2=NC, 3=C.
/// Opposite pairs: NZ<->Z, NC<->C.
static unsigned getOppositeCond(unsigned CC) {
  switch (CC) {
  case 0: return 1; // NZ -> Z
  case 1: return 0; // Z -> NZ
  case 2: return 3; // NC -> C
  case 3: return 2; // C -> NC
  default: llvm_unreachable("Invalid SM83 condition code");
  }
}

bool SM83InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                  MachineBasicBlock *&TBB,
                                  MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond,
                                  bool AllowModify) const {
  TBB = FBB = nullptr;

  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUnpredicatedTerminator(*I))
      break;

    if (isUncondBranch(*I)) {
      if (TBB) {
        // Two unconditional branches — shouldn't happen normally.
        return true;
      }
      TBB = getBranchTarget(*I);
      if (AllowModify && MBB.isLayoutSuccessor(TBB)) {
        // Remove branches to the fallthrough block.
        I->eraseFromParent();
        I = MBB.end();
        TBB = nullptr;
        continue;
      }
    } else if (isCondBranch(*I)) {
      if (TBB) {
        // We already have an unconditional branch — this conditional is first.
        FBB = TBB;
        TBB = getBranchTarget(*I);
        Cond.push_back(MachineOperand::CreateImm(getCondFromBranch(*I)));
        return false;
      }
      TBB = getBranchTarget(*I);
      Cond.push_back(MachineOperand::CreateImm(getCondFromBranch(*I)));
    } else {
      // Unknown terminator — can't analyze.
      return true;
    }
  }
  return false;
}

unsigned SM83InstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL,
                                    int *BytesAdded) const {
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  unsigned Count = 0;

  if (Cond.empty()) {
    // Unconditional branch.
    assert(!FBB && "Unconditional branch with two targets?");
    BuildMI(&MBB, DL, get(SM83::JP)).addMBB(TBB);
    if (BytesAdded) *BytesAdded = 3;
    return 1;
  }

  // Conditional branch.
  unsigned CC = Cond[0].getImm();
  BuildMI(&MBB, DL, get(SM83::JPcc)).addImm(CC).addMBB(TBB);
  Count++;
  if (BytesAdded) *BytesAdded = 3;

  if (FBB) {
    // Two-way branch: conditional + fallthrough unconditional.
    BuildMI(&MBB, DL, get(SM83::JP)).addMBB(FBB);
    Count++;
    if (BytesAdded) *BytesAdded += 3;
  }
  return Count;
}

unsigned SM83InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                    int *BytesRemoved) const {
  unsigned Count = 0;
  if (BytesRemoved) *BytesRemoved = 0;

  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (!isUncondBranch(*I) && !isCondBranch(*I))
      break;

    unsigned Size = (I->getOpcode() == SM83::JR || I->getOpcode() == SM83::JRcc)
                        ? 2 : 3;
    I->eraseFromParent();
    I = MBB.end();
    Count++;
    if (BytesRemoved) *BytesRemoved += Size;
  }
  return Count;
}

bool SM83InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 1 && "Invalid SM83 branch condition");
  unsigned CC = Cond[0].getImm();
  Cond[0].setImm(getOppositeCond(CC));
  return false;
}

} // end namespace llvm
