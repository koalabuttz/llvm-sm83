//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83SelectionDAGInfo.h"

#define GET_SDNODE_DESC
#include "SM83GenSDNodeInfo.inc"

using namespace llvm;

SM83SelectionDAGInfo::SM83SelectionDAGInfo()
    : SelectionDAGGenTargetInfo(SM83GenSDNodeInfo) {}

SM83SelectionDAGInfo::~SM83SelectionDAGInfo() = default;
