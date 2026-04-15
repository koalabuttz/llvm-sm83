//===-- SM83FixupKinds.h - SM83 Specific Fixup Entries ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83FIXUPKINDS_H
#define LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace SM83 {

// This table must be in the same order as the MCFixupKindInfo table in
// SM83AsmBackend.cpp.
enum Fixups {
  // 16-bit absolute address fixup (for CALL, JP, LD rr/nn, etc.)
  fixup_16 = FirstTargetFixupKind,

  // 8-bit PC-relative signed offset fixup (for JR instructions).
  // The offset is relative to the end of the JR instruction (PC + 2).
  fixup_pcrel_8,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // end namespace SM83
} // end namespace llvm

#endif // LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83FIXUPKINDS_H
