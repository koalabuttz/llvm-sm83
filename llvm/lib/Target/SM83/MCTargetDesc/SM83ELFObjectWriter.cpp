//===-- SM83ELFObjectWriter.cpp - SM83 ELF Writer -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83AsmBackend.h"
#include "SM83FixupKinds.h"
#include "SM83MCTargetDesc.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class SM83ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit SM83ELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64bit=*/false, OSABI, ELF::EM_SM83,
                                /*HasRelocationAddend=*/false) {}

  ~SM83ELFObjectWriter() override = default;

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    // Map fixup kinds to ELF relocation types.
    // We use generic ELF types since SM83 has no official reloc numbers.
    switch ((unsigned)Fixup.getKind()) {
    case FK_Data_1:
      return ELF::R_386_8;   // 8-bit absolute
    case SM83::fixup_pcrel_8:
      return ELF::R_386_PC8; // 8-bit PC-relative
    case FK_Data_2:
    case SM83::fixup_16:
      return ELF::R_386_16; // 16-bit absolute (generic, best match)
    case FK_Data_4:
      return ELF::R_386_32;
    default:
      llvm_unreachable("Invalid fixup kind for SM83 ELF");
    }
  }
};
} // end anonymous namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createSM83ELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<SM83ELFObjectWriter>(OSABI);
}
