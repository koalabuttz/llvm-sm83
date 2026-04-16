//===-- SM83MachineFunctionInfo.h - SM83 machine function info ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SM83_SM83MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_SM83_SM83MACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class SM83MachineFunctionInfo : public MachineFunctionInfo {
  bool IsInterruptHandler = false;

public:
  SM83MachineFunctionInfo(const Function &F,
                          const TargetSubtargetInfo *STI)
      : MachineFunctionInfo() {
    IsInterruptHandler = F.hasFnAttribute("interrupt");
  }

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<SM83MachineFunctionInfo>(*this);
  }

  bool isInterruptHandler() const { return IsInterruptHandler; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SM83_SM83MACHINEFUNCTIONINFO_H
