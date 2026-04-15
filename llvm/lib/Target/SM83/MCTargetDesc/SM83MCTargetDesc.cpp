//===-- SM83MCTargetDesc.cpp - SM83 Target Descriptions --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83MCTargetDesc.h"
#include "SM83AsmBackend.h"
#include "SM83InstPrinter.h"
#include "SM83MCAsmInfo.h"
#include "SM83MCCodeEmitter.h"
#include "TargetInfo/SM83TargetInfo.h"

#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "SM83GenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "SM83GenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "SM83GenRegisterInfo.inc"

using namespace llvm;

MCInstrInfo *llvm::createSM83MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitSM83MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createSM83MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitSM83MCRegisterInfo(X, 0);
  return X;
}

static MCSubtargetInfo *createSM83MCSubtargetInfo(const Triple &TT,
                                                   StringRef CPU,
                                                   StringRef FS) {
  return createSM83MCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCInstPrinter *createSM83MCInstPrinter(const Triple &T,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new SM83InstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCCodeEmitter *createSM83MCCodeEmitter(const MCInstrInfo &MCII,
                                              MCContext &Ctx) {
  return new SM83MCCodeEmitter(MCII, Ctx);
}

static MCAsmBackend *createSM83AsmBackendWrapper(const Target &T,
                                                  const MCSubtargetInfo &STI,
                                                  const MCRegisterInfo &MRI,
                                                  const MCTargetOptions &Options) {
  return createSM83AsmBackend(T, STI, MRI, Options);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSM83TargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfo<SM83MCAsmInfo> X(getTheSM83Target());

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(getTheSM83Target(),
                                      createSM83MCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(getTheSM83Target(),
                                    createSM83MCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(getTheSM83Target(),
                                          createSM83MCSubtargetInfo);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(getTheSM83Target(),
                                        createSM83MCInstPrinter);

  // Register the MC code emitter.
  TargetRegistry::RegisterMCCodeEmitter(getTheSM83Target(),
                                        createSM83MCCodeEmitter);

  // Register the asm backend.
  TargetRegistry::RegisterMCAsmBackend(getTheSM83Target(),
                                       createSM83AsmBackendWrapper);
}
