//===-- SM83AsmPrinter.cpp - SM83 LLVM assembly writer --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83.h"
#include "SM83MCInstLower.h"
#include "SM83Subtarget.h"
#include "SM83TargetMachine.h"
#include "TargetInfo/SM83TargetInfo.h"

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define DEBUG_TYPE "sm83-asm-printer"

using namespace llvm;

namespace {

class SM83AsmPrinter : public AsmPrinter {
public:
  SM83AsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "SM83 Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;

  static char ID;
};

} // namespace

char SM83AsmPrinter::ID = 0;

INITIALIZE_PASS(SM83AsmPrinter, "sm83-asm-printer", "SM83 Assembly Printer",
                false, false)

void SM83AsmPrinter::emitInstruction(const MachineInstr *MI) {
  SM83MCInstLower MCInstLowering(OutContext, *this);
  MCInst TmpInst;
  MCInstLowering.lowerInstruction(*MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSM83AsmPrinter() {
  RegisterAsmPrinter<SM83AsmPrinter> X(getTheSM83Target());
}
