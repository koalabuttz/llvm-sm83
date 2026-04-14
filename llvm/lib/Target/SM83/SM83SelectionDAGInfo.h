//===-- SM83SelectionDAGInfo.h - SM83 SelectionDAG Info ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SM83_SELECTION_DAG_INFO_H
#define LLVM_SM83_SELECTION_DAG_INFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

#define GET_SDNODE_ENUM
#include "SM83GenSDNodeInfo.inc"

namespace llvm {

class SM83SelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  SM83SelectionDAGInfo();
  ~SM83SelectionDAGInfo() override;
};

} // end namespace llvm

#endif // LLVM_SM83_SELECTION_DAG_INFO_H
