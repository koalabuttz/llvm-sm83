//===-- SM83MCAsmInfo.cpp - SM83 asm properties ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83MCAsmInfo.h"

using namespace llvm;

SM83MCAsmInfo::SM83MCAsmInfo(const Triple &TT, const MCTargetOptions &Options) {
  CodePointerSize = 2;
  CalleeSaveStackSlotSize = 2;
  CommentString = ";";
  UsesELFSectionDirectiveForBSS = true;
  SupportsDebugInformation = true;
}
