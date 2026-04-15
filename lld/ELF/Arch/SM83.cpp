//===- SM83.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The Sharp SM83 is an 8-bit CPU used in the Nintendo Game Boy and Game Boy
// Color.  It has a 16-bit address space (64 KB) and a Harvard-ish memory map
// split between ROM ($0000–$7FFF), VRAM ($8000–$9FFF), external RAM
// ($A000–$BFFF), WRAM ($C000–$DFFF), and HRAM ($FF80–$FFFE).
//
// SM83 ELF relocations reuse i386 relocation numbers (R_386_*) because the
// architecture has no official ELF ABI and unofficial numbers would conflict.
// The four types used:
//
//   R_386_8   (1)  — 8-bit absolute (data bytes, LDH immediates)
//   R_386_16 (20)  — 16-bit absolute LE (CALL/JP targets, 16-bit data)
//   R_386_32  (1)  — 32-bit absolute LE (rare; section-relative offsets)
//   R_386_PC8 (23) — 8-bit PC-relative signed (JR ±127 branches)
//
// For R_386_PC8 the assembler emits fixup_pcrel_8 which stores
//   written_byte = (target - fixup_loc) - 1
// because the fixup site is at instr+1 but SM83 counts the offset from instr+2
// (end of the 2-byte JR instruction).  lld receives val = target - loc (R_PC)
// and must apply the same -1 adjustment.
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class SM83 final : public TargetInfo {
public:
  SM83(Ctx &ctx);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  int64_t getImplicitAddend(const uint8_t *buf, RelType type) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

SM83::SM83(Ctx &ctx) : TargetInfo(ctx) {
  // SM83 NOP is 0x00; use four of them as the trap sequence.
  trapInstr = {0x00, 0x00, 0x00, 0x00};
}

RelExpr SM83::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  switch (type) {
  case R_386_PC8:
    return R_PC;
  default:
    return R_ABS;
  }
}

int64_t SM83::getImplicitAddend(const uint8_t *buf, RelType type) const {
  switch (type) {
  case R_386_8:
    return *buf;
  case R_386_PC8:
    return static_cast<int8_t>(*buf);
  case R_386_16:
    return read16le(buf);
  case R_386_32:
    return read32le(buf);
  default:
    InternalErr(ctx, buf) << "cannot read addend for relocation " << type;
    return 0;
  }
}

void SM83::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_386_8:
    checkUInt(ctx, loc, val, 8, rel);
    *loc = static_cast<uint8_t>(val);
    return;

  case R_386_PC8: {
    // val = target - loc  (lld R_PC expression).
    // The fixup site is at instr+1; SM83 reads the offset from instr+2, so
    // subtract 1 to match the AsmBackend::applyFixup convention.
    int64_t s = static_cast<int64_t>(val) - 1;
    checkInt(ctx, loc, s, 8, rel);
    *loc = static_cast<uint8_t>(s);
    return;
  }

  case R_386_16:
    checkUInt(ctx, loc, val, 16, rel);
    write16le(loc, static_cast<uint16_t>(val));
    return;

  case R_386_32:
    write32le(loc, static_cast<uint32_t>(val));
    return;

  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognised relocation " << rel.type;
  }
}

void elf::setSM83TargetInfo(Ctx &ctx) { ctx.target.reset(new SM83(ctx)); }
