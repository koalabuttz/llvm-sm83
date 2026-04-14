//===-- SM83CallingConv.td - Calling Conventions for SM83 ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for SM83 architecture.
// Calling convention is implemented in custom C++ code.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// SM83 Return Value Calling Convention
//===----------------------------------------------------------------------===//

def RetCC_SM83 : CallingConv<[
  CCIfType<[i8], CCAssignToReg<[A]>>,
  CCIfType<[i16], CCAssignToReg<[HL]>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved register lists.
//===----------------------------------------------------------------------===//

// Initially all registers are caller-saved (simplest approach).
def CSR_Normal : CalleeSavedRegs<(add)>;
