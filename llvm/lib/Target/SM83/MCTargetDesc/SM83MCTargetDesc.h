//===-- SM83MCTargetDesc.h - SM83 Target Descriptions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SM83_MCTARGET_DESC_H
#define LLVM_SM83_MCTARGET_DESC_H

#include "llvm/Support/DataTypes.h"

#include <memory>

namespace llvm {

class MCInstrInfo;
class Target;

MCInstrInfo *createSM83MCInstrInfo();

} // end namespace llvm

#define GET_REGINFO_ENUM
#include "SM83GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "SM83GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "SM83GenSubtargetInfo.inc"

#endif // LLVM_SM83_MCTARGET_DESC_H
