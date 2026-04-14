//===-- SM83TargetMachine.cpp - Define TargetMachine for SM83 --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83TargetMachine.h"

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#include "SM83.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"
#include "TargetInfo/SM83TargetInfo.h"

#include <optional>

namespace llvm {

static StringRef getCPU(StringRef CPU) {
  if (CPU.empty() || CPU == "generic")
    return "sm83";
  return CPU;
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

SM83TargetMachine::SM83TargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, getCPU(CPU), FS,
                               Options, getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      SubTarget(TT, std::string(getCPU(CPU)), std::string(FS), *this) {
  this->TLOF = std::make_unique<TargetLoweringObjectFileELF>();
  initAsmInfo();
}

namespace {
class SM83PassConfig : public TargetPassConfig {
public:
  SM83PassConfig(SM83TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  SM83TargetMachine &getSM83TargetMachine() const {
    return getTM<SM83TargetMachine>();
  }

  bool addInstSelector() override;
  void addPreSched2() override;
};
} // namespace

TargetPassConfig *SM83TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new SM83PassConfig(*this, PM);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSM83Target() {
  RegisterTargetMachine<SM83TargetMachine> X(getTheSM83Target());

  auto &PR = *PassRegistry::getPassRegistry();
  initializeSM83AsmPrinterPass(PR);
  initializeSM83DAGToDAGISelLegacyPass(PR);
  initializeSM83ExpandPseudoPass(PR);
}

const SM83Subtarget *SM83TargetMachine::getSubtargetImpl() const {
  return &SubTarget;
}

const SM83Subtarget *
SM83TargetMachine::getSubtargetImpl(const Function &) const {
  return &SubTarget;
}

bool SM83PassConfig::addInstSelector() {
  addPass(createSM83ISelDag(getSM83TargetMachine(), getOptLevel()));
  return false;
}

void SM83PassConfig::addPreSched2() {
  addPass(createSM83ExpandPseudoPass());
}

} // end namespace llvm
