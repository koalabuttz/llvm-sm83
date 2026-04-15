//===-- SM83AsmBackend.h - SM83 Assembler Backend ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83ASMBACKEND_H
#define LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83ASMBACKEND_H

#include "llvm/MC/MCAsmBackend.h"
#include <memory>

namespace llvm {

class MCObjectTargetWriter;
class MCSubtargetInfo;
class MCRegisterInfo;
class MCTargetOptions;
class Target;

MCAsmBackend *createSM83AsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                   const MCRegisterInfo &MRI,
                                   const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createSM83ELFObjectWriter(uint8_t OSABI);

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83ASMBACKEND_H
