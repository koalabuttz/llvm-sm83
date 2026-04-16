//===------ SemaSM83.cpp ------- SM83 target-specific routines ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis functions specific to SM83
//  (Sharp SM83 / Game Boy / Game Boy Color).
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaSM83.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclBase.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Sema/Attr.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"

namespace clang {

SemaSM83::SemaSM83(Sema &S) : SemaBase(S) {}

void SemaSM83::handleInterruptAttr(Decl *D, const ParsedAttr &AL) {
  // SM83 interrupt attribute accepts an optional vector-name string argument:
  //   __attribute__((interrupt))            -> generic ISR, placed in .text
  //   __attribute__((interrupt("vblank")))  -> placed in .isr.vblank, etc.
  if (AL.getNumArgs() > 1) {
    Diag(AL.getLoc(), diag::err_attribute_too_many_arguments) << AL << 1;
    return;
  }

  if (!isFuncOrMethodForAttrSubject(D)) {
    Diag(D->getLocation(), diag::warn_attribute_wrong_decl_type)
        << AL << AL.isRegularKeywordAttribute() << ExpectedFunction;
    return;
  }

  if (hasFunctionProto(D) && getFunctionOrMethodNumParams(D) != 0) {
    // SM83 ISRs take no parameters (hardware-invoked).
    Diag(D->getLocation(), diag::warn_interrupt_signal_attribute_invalid)
        << /*SM83*/ 4 << /*interrupt*/ 0 << 0;
    return;
  }

  if (!getFunctionOrMethodResultType(D)->isVoidType()) {
    Diag(D->getLocation(), diag::warn_interrupt_signal_attribute_invalid)
        << /*SM83*/ 4 << /*interrupt*/ 0 << 1;
    return;
  }

  StringRef Str;
  SourceLocation ArgLoc;
  if (AL.getNumArgs() == 0)
    Str = "";
  else if (!SemaRef.checkStringLiteralArgumentAttr(AL, 0, Str, &ArgLoc))
    return;

  SM83InterruptAttr::VectorKind Kind;
  if (!SM83InterruptAttr::ConvertStrToVectorKind(Str, Kind)) {
    Diag(AL.getLoc(), diag::warn_attribute_type_not_supported)
        << AL << "'" + std::string(Str) + "'";
    return;
  }

  D->addAttr(::new (getASTContext())
                 SM83InterruptAttr(getASTContext(), AL, Kind));
  // Mark the function as used so the linker retains it even if only invoked
  // by hardware (i.e. no direct call site in the TU).
  D->addAttr(UsedAttr::CreateImplicit(getASTContext()));
}

} // namespace clang
