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
//   4. Consecutive LD A, r; LD A, r → single LD A, r
//   5. LD A, r; CP s; LD A, r → LD A, r; CP s (CP doesn't clobber A)
//   6. <ALU op writing A,F>; CP 0 → remove CP 0 (ALU already set Z)
//   7. LDrr A, Rx (A dead) → remove (cascades from dead CP elimination)
//   8. Cross-block dead-load DCE: LD r, imm / LD r, r where the defined
//      register is not live at the load site — catches stray entry-block
//      loads (e.g. `ld c, 0`) that the intra-block passes can't see.
//
//===----------------------------------------------------------------------===//

#include "SM83.h"
#include "SM83InstrInfo.h"
#include "SM83Subtarget.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/LiveRegUnits.h"

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

  /// Remove duplicate LD A, r when consecutive (same source).
  bool tryRemoveDuplicateLoadA(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator &MI);

  /// Remove redundant re-load: LD A, r; CP ...; LD A, r → LD A, r; CP ...
  /// CP does not modify A, so the second LD A, r is unnecessary.
  bool tryRemoveReloadAfterCP(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator &MI);

  /// Remove CP 0 after an 8-bit ALU op that already set Z=(result==0).
  /// CP 0 sets Z=(A==0) — identical to the ALU result already in A.
  bool tryRemoveDeadCP(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator &MI);

  /// Remove LDrr A, Rx when A is dead after the load.
  /// Cascades from tryRemoveDeadCP: once the CP is gone the load has no consumer.
  bool tryRemoveDeadLoadA(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator &MI);

  /// Tail call: CALL foo; RET → JP foo (saves 1 byte + stack push/pop).
  bool tryTailCall(MachineBasicBlock &MBB,
                   MachineBasicBlock::iterator &MI);

  /// Cross-block DCE: delete LD r, imm / LD r, r whose def register is
  /// not live at the load site. Walks MBBs in post-order with
  /// LiveRegUnits so dead loads whose only users live in successor
  /// blocks (or nowhere) are caught — something the intra-block passes
  /// above can't see.
  bool eliminateCrossBlockDeadLoads(MachineFunction &MF);
};

} // namespace

char SM83PeepholeOpt::ID = 0;

INITIALIZE_PASS(SM83PeepholeOpt, DEBUG_TYPE, PASS_NAME, false, false)

bool SM83PeepholeOpt::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<SM83Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool AnyModified = false;
  bool Modified;
  do {
    Modified = false;
    for (auto &MBB : MF)
      Modified |= optimizeMBB(MBB);
    AnyModified |= Modified;
  } while (Modified);

  // Round 7: cross-block dead-load DCE. Catches stray entry-block loads
  // (`ld c, 0` in countdown loops) whose dest register is never live.
  // If anything was deleted, re-run the intra-block passes so newly
  // exposed patterns (self-copies, dead CPs) cascade.
  if (eliminateCrossBlockDeadLoads(MF)) {
    AnyModified = true;
    bool Changed;
    do {
      Changed = false;
      for (auto &MBB : MF)
        Changed |= optimizeMBB(MBB);
    } while (Changed);
  }
  return AnyModified;
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
    if (MI != E) {
      Modified |= tryRemoveDuplicateLoadA(MBB, MI);
    }
    if (MI != E) {
      Modified |= tryRemoveReloadAfterCP(MBB, MI);
    }
    if (MI != E) {
      Modified |= tryRemoveDeadCP(MBB, MI);
    }
    if (MI != E) {
      Modified |= tryRemoveDeadLoadA(MBB, MI);
    }
    if (MI != E) {
      if (tryTailCall(MBB, MI)) {
        Modified = true;
        // tryTailCall erases MI and the following RET, so MI now points
        // to the next valid instruction. Skip the stale Next assignment.
        continue;
      }
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

bool SM83PeepholeOpt::tryRemoveDuplicateLoadA(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator &MI) {
  // Pattern: LD A, r followed immediately by LD A, r (same src) → remove second.
  // Arises when pseudo expansion loads A multiple times for chained ALU ops.
  if (MI->getOpcode() != SM83::LDrr)
    return false;

  Register DstReg = MI->getOperand(0).getReg();
  Register SrcReg = MI->getOperand(1).getReg();
  if (DstReg != SM83::A)
    return false;

  auto Next = std::next(MachineBasicBlock::iterator(MI));
  if (Next == MBB.end())
    return false;

  if (Next->getOpcode() != SM83::LDrr)
    return false;

  Register NextDst = Next->getOperand(0).getReg();
  Register NextSrc = Next->getOperand(1).getReg();
  if (NextDst != SM83::A || NextSrc != SrcReg)
    return false;

  // Remove the duplicate (second) load.
  MBB.erase(Next);
  return true;
}

bool SM83PeepholeOpt::tryRemoveReloadAfterCP(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator &MI) {
  // Pattern: LD A, r; CP ...; LD A, r → remove the trailing LD A, r.
  // CP does not modify A (only sets flags), so A still holds r's value.
  if (MI->getOpcode() != SM83::LDrr)
    return false;

  Register DstReg = MI->getOperand(0).getReg();
  Register SrcReg = MI->getOperand(1).getReg();
  if (DstReg != SM83::A)
    return false;

  // Look ahead: skip CP instructions (they don't clobber A).
  auto Check = std::next(MachineBasicBlock::iterator(MI));
  while (Check != MBB.end() &&
         (Check->getOpcode() == SM83::CPr ||
          Check->getOpcode() == SM83::CPi)) {
    ++Check;
  }

  if (Check == MBB.end())
    return false;

  // If the next non-CP instruction is LD A, r with the same src, remove it.
  if (Check->getOpcode() != SM83::LDrr)
    return false;

  Register ReloadDst = Check->getOperand(0).getReg();
  Register ReloadSrc = Check->getOperand(1).getReg();
  if (ReloadDst != SM83::A || ReloadSrc != SrcReg)
    return false;

  // Also verify that SrcReg was not modified by any of the CP instructions
  // (it shouldn't be — CP only reads operands — but be defensive).
  auto Verify = std::next(MachineBasicBlock::iterator(MI));
  for (; Verify != Check; ++Verify) {
    if (Verify->definesRegister(SrcReg, TRI))
      return false;
  }

  MBB.erase(Check);
  return true;
}

bool SM83PeepholeOpt::tryRemoveDeadCP(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator &MI) {
  // Patterns eliminated:
  //   A) <ALU [A,F]>; CPi 0 → remove CPi 0
  //   B) <ALU [A,F]>; [LD r,A]; [LDri R,0]; [LD A,r]; CPr R → remove CPr R
  //      (codegen-typical form of the same redundancy)
  //
  // Any 8-bit ALU op that writes A sets Z=(result==0).
  // CP 0 (whether CPi or CPr R where R==0) re-tests A==0 — identical.
  //
  // Safety: CP 0 also sets H=1, C=0, which may differ from the ALU op's H/C.
  // Elimination is safe only when all F consumers between the CP and the next
  // F-def use Z (condition NZ=0 or Z=1), not C (NC=2 or C=3).

  if (MI == MBB.begin())
    return false;

  bool IsZeroCompare = false;
  Register CmpReg;

  if (MI->getOpcode() == SM83::CPi) {
    if (MI->getOperand(0).getImm() != 0)
      return false;
    IsZeroCompare = true;
  } else if (MI->getOpcode() == SM83::CPr) {
    CmpReg = MI->getOperand(0).getReg();
    // Scan backwards to find the most recent definition of CmpReg.
    // Accept only LDri CmpReg, 0 (i.e. CmpReg is known to hold 0).
    bool FoundDefInBlock = false;
    for (auto I = std::prev(MachineBasicBlock::iterator(MI));; --I) {
      if (I->definesRegister(CmpReg, TRI)) {
        FoundDefInBlock = true;
        if (I->getOpcode() == SM83::LDri &&
            I->getOperand(0).getReg() == CmpReg &&
            I->getOperand(1).isImm() &&
            I->getOperand(1).getImm() == 0)
          IsZeroCompare = true;
        break;
      }
      if (I == MBB.begin())
        break;
    }
    // If CmpReg is not defined within this block, check predecessors.
    // Accept only the case where every predecessor ends with a unique
    // LDri CmpReg, 0 def — the classic "ld r, 0 once before a loop" idiom.
    if (!FoundDefInBlock && !MBB.pred_empty()) {
      // Check predecessors.  Skip back-edges (Pred == &MBB) since CmpReg
      // was already verified to be unmodified inside this block — it is
      // invariant across any back-edge.  Require every non-back-edge
      // predecessor to define CmpReg as exactly 0.
      bool AllPredZero = true;
      bool AnyRealPred = false;
      for (const MachineBasicBlock *Pred : MBB.predecessors()) {
        if (Pred == &MBB)
          continue; // back-edge: CmpReg unchanged in this block → skip
        AnyRealPred = true;
        bool FoundInPred = false;
        for (auto I = Pred->rbegin(); I != Pred->rend(); ++I) {
          if (I->definesRegister(CmpReg, TRI)) {
            if (I->getOpcode() == SM83::LDri &&
                I->getOperand(0).getReg() == CmpReg &&
                I->getOperand(1).isImm() &&
                I->getOperand(1).getImm() == 0)
              FoundInPred = true;
            break;
          }
        }
        if (!FoundInPred) { AllPredZero = false; break; }
      }
      if (AnyRealPred && AllPredZero)
        IsZeroCompare = true;
    }
    if (!IsZeroCompare)
      return false;
  } else {
    return false;
  }

  // Scan backwards from the CP to verify: there exists a recent F-def whose
  // tested value equals the value in A at the CP.
  //
  // Three shapes emitted by the SM83 codegen:
  //   (a) <ALU [A,F]>; CPr R(=0): A unchanged since ALU.
  //   (b) <INC/DEC Rx [F,Rx]>; LD A,Rx; CPr R(=0): A = Rx = F's source.
  //   (c) <ALU [A,F]>; LD Rx,A; ...; LD A,Rx; CPr R(=0): save/restore of A.
  //       This is the most common: codegen spills A to a GR8 and then
  //       reloads for the compare.
  //
  // Walk backwards tracking:
  //   LastALoad: if the most recent A-def was LD A, Rx, this is Rx.
  //   SavedFromA: if we then see LD Rx, A (where Rx==LastALoad), this flag
  //               records that Rx was loaded from A — so the F-def defining A
  //               also set Rx via the save, and shape (c) applies.
  Register LastALoad;     // Rx in "LD A, Rx" — valid when LastALoadSeen
  bool LastALoadSeen = false;
  Register SaveReg;       // Rx in "LD Rx, A" after finding LastALoad
  bool SaveSeen = false;
  bool AFlagsFoundBefore = false;

  for (auto I = std::prev(MachineBasicBlock::iterator(MI));; --I) {
    if (I->definesRegister(SM83::F, TRI)) {
      if (I->definesRegister(SM83::A, TRI)) {
        if (!LastALoadSeen) {
          // Shape (a): ALU set A and F, A unchanged to this CP.
          AFlagsFoundBefore = true;
        } else if (SaveSeen && SaveReg == LastALoad) {
          // Shape (c): ALU set A; LD Rx,A saved it; LD A,Rx restored it.
          AFlagsFoundBefore = true;
        }
      } else if (LastALoadSeen && !SaveSeen) {
        // Shape (b): F-def set Rx (not A); LD A,Rx followed.
        // Verify the F-def is indeed the source for LastALoad.
        if (I->definesRegister(LastALoad, TRI))
          AFlagsFoundBefore = true;
      }
      break;
    }
    if (I->definesRegister(SM83::A, TRI)) {
      // Most recent A-definition going backwards.
      if (I->getOpcode() == SM83::LDrr &&
          I->getOperand(0).getReg() == SM83::A) {
        // LD A, Rx — a simple copy.  Remember Rx.
        LastALoad = I->getOperand(1).getReg();
        LastALoadSeen = true;
        SaveSeen = false; // reset; re-check for LD Rx,A below this point
      } else {
        // A is redefined by something other than a simple copy — bail.
        break;
      }
    } else if (LastALoadSeen && !SaveSeen &&
               I->getOpcode() == SM83::LDrr &&
               I->getOperand(0).getReg() == LastALoad &&
               I->getOperand(1).getReg() == SM83::A) {
      // LD Rx, A — a save of A into the same register we later load from.
      // This is the "save" step of shape (c).
      SaveReg = LastALoad;
      SaveSeen = true;
    } else if (LastALoadSeen &&
               I->definesRegister(LastALoad, TRI)) {
      // LastALoad is clobbered by something other than the save.
      // The value we're loading into A is not from the F-def's result.
      LastALoadSeen = false;
    }
    if (I == MBB.begin())
      break;
  }
  if (!AFlagsFoundBefore)
    return false;

  // Walk forward from the CP: every F-consumer before the next F-def must
  // read only Z (NZ=0 or Z=1), not C (NC=2 or C=3).
  for (auto I = std::next(MI); I != MBB.end(); ++I) {
    if (I->definesRegister(SM83::F, TRI))
      break;
    if (!I->readsRegister(SM83::F, TRI))
      continue;
    unsigned Op = I->getOpcode();
    if (Op != SM83::JRcc && Op != SM83::JPcc &&
        Op != SM83::CALLcc && Op != SM83::RETcc)
      return false;
    int64_t CC = I->getOperand(0).getImm();
    if (CC == 2 || CC == 3) // NC or C — reads carry
      return false;
  }

  MI = MBB.erase(MI);
  return true;
}

bool SM83PeepholeOpt::tryRemoveDeadLoadA(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator &MI) {
  // Pattern: LDrr A, Rx where A is dead after this instruction.
  // Arises when tryRemoveDeadCP eliminates the CPr/CPi that was the
  // sole consumer of A — the load becomes unreachable.
  if (MI->getOpcode() != SM83::LDrr)
    return false;
  Register DstReg = MI->getOperand(0).getReg();
  if (DstReg != SM83::A)
    return false;
  Register SrcReg = MI->getOperand(1).getReg();
  if (SrcReg == SM83::A)
    return false; // Self-copy handled by tryRemoveSelfCopy.

  // Scan forward: A must not be read before it is redefined or block ends.
  auto Check = std::next(MachineBasicBlock::iterator(MI));
  for (; Check != MBB.end(); ++Check) {
    if (Check->readsRegister(SM83::A, TRI))
      return false;
    if (Check->definesRegister(SM83::A, TRI))
      break; // A redefined first — dead here.
  }
  if (Check == MBB.end()) {
    LivePhysRegs LR;
    LR.init(*TRI);
    LR.addLiveOuts(MBB);
    if (LR.contains(SM83::A))
      return false;
  }

  MI = MBB.erase(MI);
  return true;
}

bool SM83PeepholeOpt::tryTailCall(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator &MI) {
  // Pattern: CALL foo at the end of a block, followed by a block ending in RET.
  // Replace CALL with JP (saves 1 byte + avoids push/pop of return address).
  // Constraint: only safe when CALL is the last non-terminator before RET,
  //             and no epilogue code (ADD SP, N) follows.
  if (MI->getOpcode() != SM83::CALL)
    return false;

  // The CALL must be the last instruction before the terminator region.
  auto Next = std::next(MachineBasicBlock::iterator(MI));
  if (Next == MBB.end())
    return false;

  // The next instruction must be RET.
  if (Next->getOpcode() != SM83::RET)
    return false;

  // RET must be the last instruction in the block.
  if (std::next(Next) != MBB.end())
    return false;

  // Replace CALL with JP. Copy over the CALL's implicit argument-register
  // uses (e.g. $a for an i8 arg, $bc for an i16 arg) so that downstream
  // liveness-aware passes (cross-block DCE) don't see those loads as dead.
  DebugLoc DL = MI->getDebugLoc();
  MachineOperand &Target = MI->getOperand(0);
  MachineInstrBuilder MIB = BuildMI(MBB, MI, DL, TII->get(SM83::JP)).add(Target);
  for (const MachineOperand &MO : MI->operands()) {
    if (MO.isReg() && MO.isUse())
      MIB.addReg(MO.getReg(), RegState::Implicit);
    else if (MO.isRegMask())
      MIB.addRegMask(MO.getRegMask());
  }

  // Remove the CALL and RET.
  MBB.erase(Next);
  MI = MBB.erase(MI);
  return true;
}

// Is this a load whose only effect is to define a single GPR?
// Narrow to LDri (r <- imm8) and LDrr (r <- r'). Both are pure: no memory
// access, no flag effects, no implicit defs. Safe to delete whenever the
// destination register is not live.
static bool isDeadLoadCandidate(const MachineInstr &MI) {
  unsigned Op = MI.getOpcode();
  if (Op != SM83::LDri && Op != SM83::LDrr)
    return false;
  if (MI.mayLoadOrStore() || MI.hasUnmodeledSideEffects() ||
      MI.isInlineAsm() || MI.isCall() || MI.isTerminator())
    return false;
  if (MI.getNumOperands() < 1 || !MI.getOperand(0).isReg() ||
      !MI.getOperand(0).isDef())
    return false;
  // Reject if the instruction has any implicit defs beyond the dst — the
  // generic opcode flags above should already filter these, but belt-and-
  // suspenders since flag-modifying loads would be unsafe to drop.
  for (const MachineOperand &MO : MI.implicit_operands())
    if (MO.isReg() && MO.isDef())
      return false;
  return true;
}

bool SM83PeepholeOpt::eliminateCrossBlockDeadLoads(MachineFunction &MF) {
  bool Modified = false;

  // Post-RA passes sometimes leave conservative MBB live-ins — the register
  // allocator can mark a reg as live through a block that never actually
  // reads it. That conservatism hides dead loads across blocks. Recompute
  // live-ins from scratch before querying liveness so our DCE sees the
  // true picture.
  SmallVector<MachineBasicBlock *, 8> Blocks;
  for (MachineBasicBlock &MBB : MF)
    Blocks.push_back(&MBB);
  fullyRecomputeLiveIns(Blocks);

  for (MachineBasicBlock *MBB : post_order(&MF)) {
    LiveRegUnits Units(*TRI);
    Units.addLiveOuts(*MBB);
    // Defer erases to after the reverse walk so iterators stay valid.
    SmallVector<MachineInstr *, 8> ToErase;
    for (MachineInstr &MI : reverse(*MBB)) {
      if (isDeadLoadCandidate(MI)) {
        Register Dst = MI.getOperand(0).getReg();
        if (Units.available(Dst)) {
          // Dst not live — load is dead. Do NOT stepBackward; we're
          // deleting this instruction so its operands shouldn't
          // contribute to liveness above it.
          ToErase.push_back(&MI);
          continue;
        }
      }
      Units.stepBackward(MI);
    }
    for (MachineInstr *MI : ToErase) {
      MI->eraseFromParent();
      Modified = true;
    }
  }
  return Modified;
}

FunctionPass *llvm::createSM83PeepholeOptPass() {
  return new SM83PeepholeOpt();
}
