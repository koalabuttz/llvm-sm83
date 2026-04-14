//===-- SM83.h - Top-level interface for SM83 representation -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SM83_H
#define LLVM_SM83_H

#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class SM83TargetMachine;
class FunctionPass;
class PassRegistry;

FunctionPass *createSM83ISelDag(SM83TargetMachine &TM,
                                CodeGenOptLevel OptLevel);

FunctionPass *createSM83ExpandPseudoPass();

void initializeSM83AsmPrinterPass(PassRegistry &);
void initializeSM83DAGToDAGISelLegacyPass(PassRegistry &);
void initializeSM83ExpandPseudoPass(PassRegistry &);

} // end namespace llvm

#endif // LLVM_SM83_H
