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

// Return values use the same register order as arguments.
// First i8 return → A; second → C; third → B; fourth → E; fifth → D.
// First i16 return → HL; second → BC; third → DE.
// This allows returning Option<u8>, (u8,u8), and other small aggregates
// without spilling to the stack.
def RetCC_SM83 : CallingConv<[
  CCIfType<[i8],  CCAssignToReg<[A, C, B, E, D]>>,
  CCIfType<[i16], CCAssignToReg<[HL, BC, DE]>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved register lists.
//===----------------------------------------------------------------------===//

// Initially all registers are caller-saved (simplest approach).
def CSR_Normal : CalleeSavedRegs<(add)>;
