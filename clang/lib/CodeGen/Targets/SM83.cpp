//===- SM83.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// SM83 (Sharp SM83 / Game Boy / Game Boy Color) ABI Implementation.
//===----------------------------------------------------------------------===//

namespace {

class SM83TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  SM83TargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override {
    if (GV->isDeclaration())
      return;
    const auto *FD = dyn_cast_or_null<FunctionDecl>(D);
    if (!FD)
      return;
    auto *Fn = cast<llvm::Function>(GV);

    const auto *Attr = FD->getAttr<SM83InterruptAttr>();
    if (!Attr)
      return;

    // Tell the SM83 backend to emit the ISR prologue/epilogue and RETI.
    Fn->addFnAttr("interrupt");

    // If the attribute names a specific hardware vector, pin the function
    // into the matching linker-script section. The section name is not
    // mangled; the sm83.ld script matches on it literally.
    switch (Attr->getVector()) {
    case SM83InterruptAttr::VBlank:
      Fn->setSection(".isr.vblank");
      break;
    case SM83InterruptAttr::LCD:
      Fn->setSection(".isr.lcd");
      break;
    case SM83InterruptAttr::Timer:
      Fn->setSection(".isr.timer");
      break;
    case SM83InterruptAttr::Serial:
      Fn->setSection(".isr.serial");
      break;
    case SM83InterruptAttr::Joypad:
      Fn->setSection(".isr.joypad");
      break;
    case SM83InterruptAttr::Generic:
      // No section override; handler lives in .text like any other function.
      // A linker-script stub at $40/$48/... must dispatch to it manually.
      break;
    }
  }
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createSM83TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<SM83TargetCodeGenInfo>(CGM.getTypes());
}
