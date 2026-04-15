//===-- SM83RegisterInfo.td - SM83 Register defs -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Declarations that describe the SM83 register file.
//
//===----------------------------------------------------------------------===//

class SM83Reg<bits<16> num, string name, list<Register> subregs = []>
    : RegisterWithSubRegs<name, subregs> {
  field bits<16> Num = num;
  let HWEncoding = num;
  let Namespace = "SM83";
  let SubRegs = subregs;
}

// Subregister indices for 16-bit register pairs.
let Namespace = "SM83" in {
  def sub_lo : SubRegIndex<8>;
  def sub_hi : SubRegIndex<8, 8>;
}

//===----------------------------------------------------------------------===//
//  8-bit registers
//===----------------------------------------------------------------------===//

// HW encoding matches the SM83 register encoding in instructions:
// B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7
def B : SM83Reg<0, "b">, DwarfRegNum<[0]>;
def C : SM83Reg<1, "c">, DwarfRegNum<[1]>;
def D : SM83Reg<2, "d">, DwarfRegNum<[2]>;
def E : SM83Reg<3, "e">, DwarfRegNum<[3]>;
def H : SM83Reg<4, "h">, DwarfRegNum<[4]>;
def L : SM83Reg<5, "l">, DwarfRegNum<[5]>;
def A : SM83Reg<7, "a">, DwarfRegNum<[7]>;
def F : SM83Reg<6, "f">, DwarfRegNum<[6]>;

// Stack pointer sub-registers (not directly accessible but needed for
// the SP register pair definition).
def SPL : SM83Reg<8, "SPL">, DwarfRegNum<[8]>;
def SPH : SM83Reg<9, "SPH">, DwarfRegNum<[9]>;

//===----------------------------------------------------------------------===//
//  16-bit register pairs
//===----------------------------------------------------------------------===//

let SubRegIndices = [sub_lo, sub_hi], CoveredBySubRegs = 1 in {
  def BC : SM83Reg<0, "bc", [C, B]>, DwarfRegNum<[0]>;
  def DE : SM83Reg<1, "de", [E, D]>, DwarfRegNum<[2]>;
  def HL : SM83Reg<2, "hl", [L, H]>, DwarfRegNum<[4]>;
  def AF : SM83Reg<3, "af", [F, A]>, DwarfRegNum<[6]>;
  def SP : SM83Reg<4, "sp", [SPL, SPH]>, DwarfRegNum<[8]>;
}

//===----------------------------------------------------------------------===//
//  Register Classes
//===----------------------------------------------------------------------===//

// GR8: general 8-bit registers (used in LD r,r' and CB-prefix ops).
// Does NOT include A.
def GR8 : RegisterClass<"SM83", [i8], 8, (add B, C, D, E, H, L)>;

// GPR8: all 8-bit GPRs including A.
def GPR8 : RegisterClass<"SM83", [i8], 8, (add A, B, C, D, E, H, L)>;

// Acc: accumulator only — for ALU ops that require A.
def Acc : RegisterClass<"SM83", [i8], 8, (add A)>;

// GR16: general 16-bit register pairs (BC, DE, HL).
def GR16 : RegisterClass<"SM83", [i16], 8, (add BC, DE, HL)>;

// GR16NoHL: 16-bit pairs excluding HL — used for pseudos that clobber HL
// during expansion (e.g., LOAD_FI16, STORE_FI16).
def GR16NoHL : RegisterClass<"SM83", [i16], 8, (add BC, DE)>;

// GPR16: all pushable/poppable 16-bit pairs.
def GPR16 : RegisterClass<"SM83", [i16], 8, (add BC, DE, HL, AF)>;

// Ptr: the only general indirect register pair.
def Ptr : RegisterClass<"SM83", [i16], 8, (add HL)>;

// Stack pointer register class.
def GPRSP : RegisterClass<"SM83", [i16], 8, (add SP)>;

// Flags register — not directly usable.
def FLAGS : RegisterClass<"SM83", [i8], 8, (add F)> {
  let CopyCost = -1;
}
