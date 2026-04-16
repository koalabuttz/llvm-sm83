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
    // SM83 has no official ELF ABI.  We reuse i386 relocation numbers because:
    //   (a) the values (8→22, 16→20, PC8→23, 32→1) are consistent across
    //       this assembler, lld/ELF/Arch/SM83.cpp, and any pre-built rustc
    //       binaries that target sm83-unknown-none;
    //   (b) the SM83-specific numbers (R_SM83_8 = 1, R_SM83_16 = 2, …) would
    //       collide with R_386_32/PC32/… making backward compatibility impossible
    //       without a full toolchain rebuild.
    // When an official SM83 ELF ABI is ratified, switch to R_SM83_* and rebuild.
    switch ((unsigned)Fixup.getKind()) {
    case FK_Data_1:
      return ELF::R_386_8;
    case SM83::fixup_pcrel_8:
      return ELF::R_386_PC8;
    case FK_Data_2:
    case SM83::fixup_16:
      return ELF::R_386_16;
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
