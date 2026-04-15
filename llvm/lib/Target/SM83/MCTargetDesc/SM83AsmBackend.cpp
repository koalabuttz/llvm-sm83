//===-- SM83AsmBackend.cpp - SM83 Assembler Backend -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83AsmBackend.h"
#include "SM83FixupKinds.h"
#include "SM83MCTargetDesc.h"

#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class SM83AsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  SM83AsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  ~SM83AsmBackend() override = default;

  // -------------------------------------------------------------------------
  // Fixup info table — must match SM83FixupKinds.h order.
  // -------------------------------------------------------------------------
  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    const static MCFixupKindInfo Infos[SM83::NumTargetFixupKinds] = {
        // name               offset  bits  flags
        {"fixup_16",             0,    16,   0},
        {"fixup_pcrel_8",        0,     8,   0},
    };
    static_assert(std::size(Infos) == SM83::NumTargetFixupKinds,
                  "Not all fixup kinds added to Infos array");

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);

    return Infos[Kind - FirstTargetFixupKind];
  }

  // -------------------------------------------------------------------------
  // applyFixup — patch the encoded bytes with the resolved fixup value.
  //
  // NOTE: `Data` is already positioned at the fixup location by the caller
  // (i.e., Data = fragment_contents + fixup_offset).  Write to Data[0],
  // Data[1], etc. — do NOT add Fixup.getOffset() again.
  // -------------------------------------------------------------------------
  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    maybeAddReloc(F, Fixup, Target, Value, IsResolved);

    unsigned Kind = Fixup.getKind();

    switch (Kind) {
    case SM83::fixup_16:
    case FK_Data_2:
      // 16-bit little-endian absolute address.
      Data[0] = Value & 0xFF;
      Data[1] = (Value >> 8) & 0xFF;
      return;

    case SM83::fixup_pcrel_8: {
      // JR instruction: the offset is relative to the end of the 2-byte
      // instruction.  The assembler computed:
      //   value = target_addr - fixup_addr   (fixup_addr = instr_start + 1)
      // But SM83 expects:
      //   offset = target_addr - (instr_start + 2) = value - 1
      int64_t Adjusted = static_cast<int64_t>(Value) - 1;
      if (Adjusted < -128 || Adjusted > 127)
        getContext().reportError(Fixup.getLoc(),
                                 "SM83 JR offset out of range [-128, 127]");
      Data[0] = static_cast<uint8_t>(Adjusted & 0xFF);
      return;
    }

    case FK_Data_1:
      Data[0] = Value & 0xFF;
      return;

    case FK_Data_4:
      Data[0] = Value & 0xFF;
      Data[1] = (Value >> 8) & 0xFF;
      Data[2] = (Value >> 16) & 0xFF;
      Data[3] = (Value >> 24) & 0xFF;
      return;

    default:
      llvm_unreachable("Unknown SM83 fixup kind in applyFixup");
    }
  }

  // -------------------------------------------------------------------------
  // writeNopData — emit NOP (0x00) bytes for alignment padding.
  // -------------------------------------------------------------------------
  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    while (Count--)
      OS.write('\x00');
    return true;
  }

  // -------------------------------------------------------------------------
  // Instruction relaxation: JR (±127 B, 2 bytes) → JP (16-bit, 3 bytes)
  // -------------------------------------------------------------------------

  bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                         const MCSubtargetInfo &STI) const override {
    return Opcode == SM83::JR || Opcode == SM83::JRcc;
  }

  bool fixupNeedsRelaxationAdvanced(const MCFragment &, const MCFixup &Fixup,
                                    const MCValue &, uint64_t UnsignedValue,
                                    bool Resolved) const override {
    if ((unsigned)Fixup.getKind() != SM83::fixup_pcrel_8)
      return false;
    if (!Resolved)
      return true;
    int64_t Value = static_cast<int64_t>(UnsignedValue) - 1;
    return !isInt<8>(Value);
  }

  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override {
    switch (Inst.getOpcode()) {
    default:
      llvm_unreachable("relaxInstruction called on non-JR SM83 instruction");
    case SM83::JR:
      Inst.setOpcode(SM83::JP);
      return;
    case SM83::JRcc:
      Inst.setOpcode(SM83::JPcc);
      return;
    }
  }

  // -------------------------------------------------------------------------
  // Object target writer
  // -------------------------------------------------------------------------
  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createSM83ELFObjectWriter(OSABI);
  }
};

} // end anonymous namespace

MCAsmBackend *llvm::createSM83AsmBackend(const Target &T,
                                         const MCSubtargetInfo &STI,
                                         const MCRegisterInfo &MRI,
                                         const MCTargetOptions &Options) {
  return new SM83AsmBackend(STI, ELF::ELFOSABI_STANDALONE);
}
