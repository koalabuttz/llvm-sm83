//===-- SM83.cpp - Emit LLVM Code for SM83 builtins -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit SM83 builtin calls as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CGBuiltin.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/IR/Constants.h"

using namespace clang;
using namespace CodeGen;
using namespace llvm;

// Parse ".romx.bankN" (N in 1..511); returns -1 on any other form.
// Kept local to Clang CodeGen to avoid a cross-component dependency; the
// mirror of this logic lives in SM83ISelLowering.cpp::parseRomxBank.
static int parseRomxBank(StringRef SectionName) {
  StringRef Prefix = ".romx.bank";
  if (!SectionName.starts_with(Prefix))
    return -1;
  StringRef NumStr = SectionName.drop_front(Prefix.size());
  unsigned N = 0;
  if (NumStr.getAsInteger(10, N))
    return -1;
  if (N < 1 || N > 511)
    return -1;
  return static_cast<int>(N);
}

// Walk through unary plus/cast/parens/AddrOf wrappers to find a direct
// DeclRefExpr to a FunctionDecl. Returns nullptr if the argument isn't a
// compile-time function reference.
static const clang::FunctionDecl *findReferencedFunction(const clang::Expr *E) {
  E = E->IgnoreParenCasts();
  if (const auto *UO = dyn_cast<clang::UnaryOperator>(E))
    if (UO->getOpcode() == clang::UO_AddrOf)
      E = UO->getSubExpr()->IgnoreParenCasts();
  if (const auto *DRE = dyn_cast<clang::DeclRefExpr>(E))
    return dyn_cast<clang::FunctionDecl>(DRE->getDecl());
  return nullptr;
}

Value *CodeGenFunction::EmitSM83BuiltinExpr(unsigned BuiltinID,
                                            const CallExpr *E) {
  switch (BuiltinID) {
  case SM83::BI__builtin_sm83_bank_of: {
    const FunctionDecl *FD = findReferencedFunction(E->getArg(0));
    if (!FD || !FD->hasAttr<SectionAttr>()) {
      CGM.Error(E->getExprLoc(),
                "argument to __builtin_sm83_bank_of must be a direct reference "
                "to a function placed in .romx.bankN via "
                "__attribute__((section(...)))");
      return llvm::ConstantInt::get(Int8Ty, 0);
    }
    StringRef Section = FD->getAttr<SectionAttr>()->getName();
    int Bank = parseRomxBank(Section);
    if (Bank < 0) {
      CGM.Error(E->getExprLoc(),
                "__builtin_sm83_bank_of requires the function's section to be "
                "\".romx.bankN\" with N in 1..511");
      return llvm::ConstantInt::get(Int8Ty, 0);
    }
    // Returns the low 8 bits of the bank number. High bit (for MBC5 banks
    // 256..511) is not returned here — callers that target high banks must
    // use the 9-bit-aware GB_FAR_CALL_*_MBC5 family, which combines the
    // result of this builtin with an additional bit from a helper.
    return llvm::ConstantInt::get(Int8Ty, Bank & 0xFF);
  }
  }
  return nullptr;
}
