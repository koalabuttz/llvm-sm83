//===-- SM83Subtarget.cpp - SM83 Subtarget Information ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83Subtarget.h"

#include "llvm/MC/TargetRegistry.h"

#include "SM83.h"
#include "SM83TargetMachine.h"

#define DEBUG_TYPE "sm83-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "SM83GenSubtargetInfo.inc"

namespace llvm {

SM83Subtarget::SM83Subtarget(const Triple &TT, const std::string &CPU,
                             const std::string &FS,
                             const SM83TargetMachine &TM)
    : SM83GenSubtargetInfo(TT, CPU, /*TuneCPU*/ CPU, FS), InstrInfo(*this),
      TLInfo(TM, initializeSubtargetDependencies(CPU, FS, TM)) {}

SM83Subtarget &
SM83Subtarget::initializeSubtargetDependencies(StringRef CPU, StringRef FS,
                                               const TargetMachine &TM) {
  ParseSubtargetFeatures(CPU, /*TuneCPU*/ CPU, FS);
  return *this;
}

} // end namespace llvm
