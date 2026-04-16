//===--- SM83.cpp - SM83 ToolChain Implementation -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace llvm::opt;

// --- SM83ToolChain ---

SM83ToolChain::SM83ToolChain(const Driver &D, const llvm::Triple &Triple,
                             const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {}

Tool *SM83ToolChain::buildLinker() const {
  return new tools::sm83::Linker(*this);
}

void SM83ToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                              ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc) ||
      DriverArgs.hasArg(options::OPT_nostdlibinc))
    return;

  // Clang's generic headers (stddef.h, stdint.h, …) live at
  // <resource-dir>/include; the SM83 shims (string.h, gb.h) sit under
  // <resource-dir>/include/sm83. Add the SM83 dir with higher precedence so
  // our freestanding <string.h> is preferred over any accidental host copy.
  SmallString<128> P(getDriver().ResourceDir);
  llvm::sys::path::append(P, "include", "sm83");
  addSystemInclude(DriverArgs, CC1Args, P);
}

// --- SM83 Linker ---

void sm83::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                const InputInfo &Output,
                                const InputInfoList &Inputs,
                                const ArgList &Args,
                                const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // Add library search paths.
  getToolChain().AddFilePathLibArgs(Args, CmdArgs);

  // Add input files.
  AddLinkerInputs(getToolChain(), Inputs, Args, CmdArgs, JA);

  const char *Exec = Args.MakeArgString(getToolChain().GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}
