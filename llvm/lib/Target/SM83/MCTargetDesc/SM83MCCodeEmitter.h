//===-- SM83MCCodeEmitter.h - SM83 Code Emitter -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the SM83MCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83MCCODEEMITTER_H
#define LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83MCCODEEMITTER_H

#include "SM83FixupKinds.h"
#include "llvm/MC/MCCodeEmitter.h"

namespace llvm {

class MCContext;
class MCInst;
class MCInstrInfo;
class MCOperand;
class MCSubtargetInfo;

class SM83MCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;
  const MCInstrInfo &MCII;

public:
  SM83MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
      : Ctx(Ctx), MCII(MCII) {}

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  // Emit a 16-bit little-endian value, adding a fixup if the operand is an
  // expression.
  void emitImm16(const MCOperand &MO, uint32_t FixupOffset,
                 SmallVectorImpl<char> &CB,
                 SmallVectorImpl<MCFixup> &Fixups) const;

  // Emit a PC-relative 8-bit signed offset, adding a fixup if needed.
  // The JR offset is relative to the end of the 2-byte instruction.
  void emitPCRel8(const MCOperand &MO, uint32_t FixupOffset,
                  SmallVectorImpl<char> &CB,
                  SmallVectorImpl<MCFixup> &Fixups) const;

  SM83MCCodeEmitter(const SM83MCCodeEmitter &) = delete;
  void operator=(const SM83MCCodeEmitter &) = delete;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SM83_MCTARGETDESC_SM83MCCODEEMITTER_H
