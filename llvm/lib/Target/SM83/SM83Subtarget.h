//===-- SM83Subtarget.h - Define Subtarget for the SM83 ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SM83_SUBTARGET_H
#define LLVM_SM83_SUBTARGET_H

#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#include "SM83FrameLowering.h"
#include "SM83ISelLowering.h"
#include "SM83InstrInfo.h"
#include "SM83SelectionDAGInfo.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

#define GET_SUBTARGETINFO_HEADER
#include "SM83GenSubtargetInfo.inc"

namespace llvm {

class SM83TargetMachine;

class SM83Subtarget : public SM83GenSubtargetInfo {
public:
  SM83Subtarget(const Triple &TT, const std::string &CPU,
                const std::string &FS, const SM83TargetMachine &TM);

  const SM83InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const SM83TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const SM83SelectionDAGInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
  const SM83RegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  SM83Subtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS,
                                                  const TargetMachine &TM);

private:
  SM83InstrInfo InstrInfo;
  SM83FrameLowering FrameLowering;
  SM83TargetLowering TLInfo;
  SM83SelectionDAGInfo TSInfo;
};

} // end namespace llvm

#endif // LLVM_SM83_SUBTARGET_H
