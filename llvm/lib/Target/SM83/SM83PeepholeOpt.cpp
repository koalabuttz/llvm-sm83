//===-- SM83PeepholeOpt.cpp - SM83 peephole optimizations ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass performs SM83-specific peephole optimizations after pseudo
// expansion and scheduling. Targets patterns that arise from the
// accumulator-centric ALU and register-pair architecture.
//
// Patterns optimized:
//   1. LD A, 0 → XOR A  (2 bytes → 1 byte)
//   2. LD r, r (self-copy) → eliminated
//   3. LD A, r; <ALU A, ...>; LD r, A → remove trailing LD when r is dead
//
//===----------------------------------------------------------------------===//

#include "SM83.h"
#include "SM83InstrInfo.h"
#include "SM83Subtarget.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/LivePhysRegs.h"

using namespace llvm;

#define DEBUG_TYPE "sm83-peephole"
#define PASS_NAME "SM83 peephole optimization"

namespace {

class SM83PeepholeOpt : public MachineFunctionPass {
public:
  static char ID;
  SM83PeepholeOpt() : MachineFunctionPass(ID) {
    initializeSM83PeepholeOptPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const SM83InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

  bool optimizeMBB(MachineBasicBlock &MBB);

  /// Replace LD A, 0 with XOR A (saves 1 byte).
  bool tryOptimizeLoadZero(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator &MI);

  /// Remove self-copy LD r, r instructions.
  bool tryRemoveSelfCopy(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MI);

  /// Remove redundant store-back: LD A, r; <op>; LD r, A when r is dead.
  bool tryRemoveRedundantStoreBack(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator &MI);
};

} // namespace

char SM83PeepholeOpt::ID = 0;

INITIALIZE_PASS(SM83PeepholeOpt, DEBUG_TYPE, PASS_NAME, false, false)

bool SM83PeepholeOpt::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<SM83Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= optimizeMBB(MBB);
  return Modified;
}

bool SM83PeepholeOpt::optimizeMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (auto MI = MBB.begin(), E = MBB.end(); MI != E;) {
    auto Next = std::next(MI);
    Modified |= tryOptimizeLoadZero(MBB, MI);
    if (MI != E) {
      Modified |= tryRemoveSelfCopy(MBB, MI);
    }
    if (MI != E) {
      Modified |= tryRemoveRedundantStoreBack(MBB, MI);
    }
    MI = Next;
  }
  return Modified;
}

bool SM83PeepholeOpt::tryOptimizeLoadZero(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator &MI) {
  // LD A, 0 (LDri A, 0) → XOR A (XORr A)
  // LDri is 2 bytes (opcode + immediate), XOR A is 1 byte.
  // Note: XOR A also clears flags (sets Z=1, N=0, H=0, C=0).
  // This is safe as long as flags are not live after this instruction.
  if (MI->getOpcode() != SM83::LDri)
    return false;

  Register DstReg = MI->getOperand(0).getReg();
  if (DstReg != SM83::A)
    return false;

  const MachineOperand &ImmOp = MI->getOperand(1);
  if (!ImmOp.isImm() || ImmOp.getImm() != 0)
    return false;

  // Check if flags (F register) are live after this instruction.
  // If flags are live, we can't replace with XOR A because it clobbers flags.
  // Use a simple check: scan forward to the next flag-using/defining instruction.
  bool FlagsLive = false;
  auto CheckMI = std::next(MI);
  for (; CheckMI != MBB.end(); ++CheckMI) {
    if (CheckMI->readsRegister(SM83::F, TRI)) {
      FlagsLive = true;
      break;
    }
    if (CheckMI->definesRegister(SM83::F, TRI))
      break; // Flags are redefined before being read — safe.
  }
  // If we reached the end of the block without finding a flag def/use,
  // check if F is live-out of the block.
  if (CheckMI == MBB.end()) {
    LivePhysRegs LiveRegs;
    LiveRegs.init(*TRI);
    LiveRegs.addLiveOuts(MBB);
    if (LiveRegs.contains(SM83::F))
      FlagsLive = true;
  }

  if (FlagsLive)
    return false;

  // Replace with XOR A.
  DebugLoc DL = MI->getDebugLoc();
  BuildMI(MBB, MI, DL, TII->get(SM83::XORr)).addReg(SM83::A);
  MI = MBB.erase(MI);
  return true;
}

bool SM83PeepholeOpt::tryRemoveSelfCopy(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator &MI) {
  // LD r, r (self-copy) is a no-op; remove it.
  if (MI->getOpcode() != SM83::LDrr)
    return false;

  Register DstReg = MI->getOperand(0).getReg();
  Register SrcReg = MI->getOperand(1).getReg();
  if (DstReg != SrcReg)
    return false;

  MI = MBB.erase(MI);
  return true;
}

bool SM83PeepholeOpt::tryRemoveRedundantStoreBack(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator &MI) {
  // Pattern: LD r, A  where r is dead after this point.
  // This arises from accumulator-centric ALU expansion:
  //   LD A, r; <ALU op>; LD r, A
  // When r is not used again, the final LD r, A is redundant.
  if (MI->getOpcode() != SM83::LDrr)
    return false;

  Register DstReg = MI->getOperand(0).getReg();
  Register SrcReg = MI->getOperand(1).getReg();

  // Only consider LD r, A where r != A.
  if (SrcReg != SM83::A || DstReg == SM83::A)
    return false;

  // Check if DstReg is dead after this instruction.
  if (!MI->getOperand(0).isDead()) {
    // The register allocator may not have set the dead flag.
    // Do a simple liveness check.
    bool DstUsedLater = false;
    auto CheckMI = std::next(MachineBasicBlock::iterator(MI));
    for (; CheckMI != MBB.end(); ++CheckMI) {
      if (CheckMI->readsRegister(DstReg, TRI)) {
        DstUsedLater = true;
        break;
      }
      if (CheckMI->definesRegister(DstReg, TRI))
        break;
    }
    if (DstUsedLater)
      return false;
    // Also check live-outs.
    if (CheckMI == MBB.end()) {
      LivePhysRegs LiveRegs;
      LiveRegs.init(*TRI);
      LiveRegs.addLiveOuts(MBB);
      if (LiveRegs.contains(DstReg))
        return false;
    }
  }

  MI = MBB.erase(MI);
  return true;
}

FunctionPass *llvm::createSM83PeepholeOptPass() {
  return new SM83PeepholeOpt();
}
