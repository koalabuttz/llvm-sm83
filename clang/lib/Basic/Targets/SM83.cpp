//===--- SM83.cpp - Implement SM83 target feature support -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements SM83 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "SM83.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"

using namespace clang;
using namespace clang::targets;

const char *const SM83TargetInfo::GCCRegNames[] = {
    "a", "b", "c", "d", "e", "h", "l", "bc", "de", "hl", "sp"};

ArrayRef<const char *> SM83TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void SM83TargetInfo::getTargetDefines(const LangOptions &Opts,
                                      MacroBuilder &Builder) const {
  Builder.defineMacro("__SM83__");
  Builder.defineMacro("__sm83__");
  Builder.defineMacro("__GAMEBOY__");
}

static constexpr int NumBuiltins =
    clang::SM83::LastTSBuiltin - Builtin::FirstTSBuiltin;

static constexpr llvm::StringTable BuiltinStrings =
    CLANG_BUILTIN_STR_TABLE_START
#define BUILTIN CLANG_BUILTIN_STR_TABLE
#define TARGET_BUILTIN CLANG_TARGET_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsSM83.def"
    ;

static constexpr auto BuiltinInfos = Builtin::MakeInfos<NumBuiltins>({
#define BUILTIN CLANG_BUILTIN_ENTRY
#define TARGET_BUILTIN CLANG_TARGET_BUILTIN_ENTRY
#include "clang/Basic/BuiltinsSM83.def"
});

llvm::SmallVector<Builtin::InfosShard>
SM83TargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}
