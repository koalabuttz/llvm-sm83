//===-- SM83TargetInfo.cpp - SM83 Target Implementation -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/SM83TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
Target &getTheSM83Target() {
  static Target TheSM83Target;
  return TheSM83Target;
}
} // namespace llvm

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSM83TargetInfo() {
  llvm::RegisterTarget<llvm::Triple::sm83> X(
      llvm::getTheSM83Target(), "sm83", "Sharp SM83 (Game Boy)", "SM83");
}
