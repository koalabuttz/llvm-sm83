//===----- SemaSM83.h ------- SM83 target-specific routines ---*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file declares semantic analysis functions specific to SM83
/// (Sharp SM83 / Game Boy / Game Boy Color).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMASM83_H
#define LLVM_CLANG_SEMA_SEMASM83_H

#include "clang/AST/ASTFwd.h"
#include "clang/Sema/SemaBase.h"

namespace clang {
class ParsedAttr;

class SemaSM83 : public SemaBase {
public:
  SemaSM83(Sema &S);

  void handleInterruptAttr(Decl *D, const ParsedAttr &AL);
};

} // namespace clang

#endif // LLVM_CLANG_SEMA_SEMASM83_H
