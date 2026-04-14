//===-- SM83InstrInfo.td - SM83 Instruction defs ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the SM83 instruction set in TableGen format.
//
//===----------------------------------------------------------------------===//

include "SM83InstrFormats.td"

//===----------------------------------------------------------------------===//
// SM83 Type Profiles
//===----------------------------------------------------------------------===//

def SDT_SM83CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_SM83CallSeqEnd : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_SM83Call : SDTypeProfile<0, -1, [SDTCisVT<0, iPTR>]>;
def SDT_SM83Wrapper : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>, SDTCisPtrTy<0>]>;
def SDT_SM83Brcond : SDTypeProfile<0, 2, [SDTCisVT<0, OtherVT>,
                                           SDTCisVT<1, i8>]>;
def SDT_SM83Cmp : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>]>;
def SDT_SM83SelectCC : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                             SDTCisSameAs<1, 2>,
                                             SDTCisVT<3, i8>]>;

//===----------------------------------------------------------------------===//
// SM83 specific SDNodes
//===----------------------------------------------------------------------===//

def SM83ret : SDNode<"SM83ISD::RET", SDTNone,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def SM83callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_SM83CallSeqStart,
                               [SDNPHasChain, SDNPOutGlue]>;
def SM83callseq_end : SDNode<"ISD::CALLSEQ_END", SDT_SM83CallSeqEnd,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def SM83call : SDNode<"SM83ISD::CALL", SDT_SM83Call,
                      [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;

def SM83Wrapper : SDNode<"SM83ISD::WRAPPER", SDT_SM83Wrapper>;

def SM83brcond : SDNode<"SM83ISD::BRCOND", SDT_SM83Brcond,
                        [SDNPHasChain, SDNPInGlue]>;

def SM83cmp : SDNode<"SM83ISD::CMP", SDT_SM83Cmp, [SDNPOutGlue]>;

def SM83selectcc : SDNode<"SM83ISD::SELECT_CC", SDT_SM83SelectCC,
                          [SDNPInGlue]>;

//===----------------------------------------------------------------------===//
// Operand definitions
//===----------------------------------------------------------------------===//

def imm8 : Operand<i8>;
def imm16 : Operand<i16>;
def imm8s : Operand<i8> {
  let PrintMethod = "printOperand";
}
def brtarget : Operand<OtherVT>;

// Condition codes for SM83: NZ=0, Z=1, NC=2, C=3
def SM83condcode : Operand<i8> {
  let PrintMethod = "printOperand";
}

//===----------------------------------------------------------------------===//
// Pseudo instructions
//===----------------------------------------------------------------------===//

let Defs = [SP], Uses = [SP], hasSideEffects = 0 in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i16imm:$amt, i16imm:$amt2),
                              "# ADJCALLSTACKDOWN $amt",
                              [(SM83callseq_start timm:$amt, timm:$amt2)]>;
def ADJCALLSTACKUP : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                            "# ADJCALLSTACKUP $amt1",
                            [(SM83callseq_end timm:$amt1, timm:$amt2)]>;
}

// SELECT_CC pseudo — expanded in ISelLowering.
let usesCustomInserter = 1 in
def SELECT8 : Pseudo<(outs GPR8:$dst),
                     (ins GPR8:$src1, GPR8:$src2, i8imm:$cc),
                     "# SELECT8 $dst, $src1, $src2, $cc",
                     [(set i8:$dst,
                       (SM83selectcc i8:$src1, i8:$src2, imm:$cc))]>;

// Load/Store pseudos for global address access.
// These expand to: LD HL, addr / LD r, (HL)  or  LD HL, addr / LD (HL), r
let mayLoad = 1, hasSideEffects = 0 in
def LOAD8 : Pseudo<(outs GPR8:$dst), (ins i16imm:$addr),
                   "# LOAD8 $dst, [$addr]",
                   [(set i8:$dst, (load (SM83Wrapper tglobaladdr:$addr)))]>;

let mayStore = 1, hasSideEffects = 0 in
def STORE8 : Pseudo<(outs), (ins GPR8:$src, i16imm:$addr),
                    "# STORE8 [$addr], $src",
                    [(store i8:$src, (SM83Wrapper tglobaladdr:$addr))]>;

// Load/store from frame index (stack-allocated local variables).
// Selected manually in ISelDAGToDAG for loads/stores to frame indices.
let mayLoad = 1, hasSideEffects = 0 in
def LOAD_FI8 : Pseudo<(outs GPR8:$dst), (ins i16imm:$fi),
                      "# LOAD_FI8 $dst, [$fi]", []>;

let mayStore = 1, hasSideEffects = 0 in
def STORE_FI8 : Pseudo<(outs), (ins GPR8:$src, i16imm:$fi),
                       "# STORE_FI8 [$fi], $src", []>;

// Stack spill pseudos — expanded to LDHLSP + LD r,(HL) / LD (HL),r.
// Used by register allocator for spills, not matched by patterns.
let mayLoad = 1, hasSideEffects = 0, mayStore = 0 in
def LOAD_STACK8 : Pseudo<(outs GPR8:$dst), (ins i16imm:$offset),
                         "# LOAD_STACK8 $dst, SP+$offset", []>;

let mayStore = 1, hasSideEffects = 0, mayLoad = 0 in
def STORE_STACK8 : Pseudo<(outs), (ins GPR8:$src, i16imm:$offset),
                          "# STORE_STACK8 SP+$offset, $src", []>;

// CMP pseudo — expanded to LD A, src1 / CP src2. Sets flags.
let Defs = [F] in
def CMP8rr : Pseudo<(outs), (ins GPR8:$lhs, GPR8:$rhs),
                    "# CMP8 $lhs, $rhs",
                    [(SM83cmp i8:$lhs, i8:$rhs)]>;

// BRCOND pseudo — conditional branch based on flags.
let isBranch = 1, isTerminator = 1, Uses = [F] in
def BRCONDcc : Pseudo<(outs), (ins brtarget:$target, i8imm:$cc),
                      "# BRCOND $target, $cc",
                      [(SM83brcond bb:$target, imm:$cc)]>;

//===----------------------------------------------------------------------===//
// i8 ALU pseudo instructions (expanded to LD A + ALU + result in A)
// These exist because SM83's ALU works through the accumulator, but
// the register allocator needs to see two-operand instructions.
//===----------------------------------------------------------------------===//

let Defs = [F] in {
def ADD8rr : Pseudo<(outs GPR8:$dst), (ins GPR8:$src1, GPR8:$src2),
                    "# ADD8 $dst, $src1, $src2",
                    [(set i8:$dst, (add i8:$src1, i8:$src2))]>;
def SUB8rr : Pseudo<(outs GPR8:$dst), (ins GPR8:$src1, GPR8:$src2),
                    "# SUB8 $dst, $src1, $src2",
                    [(set i8:$dst, (sub i8:$src1, i8:$src2))]>;
def AND8rr : Pseudo<(outs GPR8:$dst), (ins GPR8:$src1, GPR8:$src2),
                    "# AND8 $dst, $src1, $src2",
                    [(set i8:$dst, (and i8:$src1, i8:$src2))]>;
def OR8rr : Pseudo<(outs GPR8:$dst), (ins GPR8:$src1, GPR8:$src2),
                   "# OR8 $dst, $src1, $src2",
                   [(set i8:$dst, (or i8:$src1, i8:$src2))]>;
def XOR8rr : Pseudo<(outs GPR8:$dst), (ins GPR8:$src1, GPR8:$src2),
                    "# XOR8 $dst, $src1, $src2",
                    [(set i8:$dst, (xor i8:$src1, i8:$src2))]>;
def SHL8r : Pseudo<(outs GPR8:$dst), (ins GPR8:$src, GPR8:$amt),
                   "# SHL8 $dst, $src, $amt",
                   [(set i8:$dst, (shl i8:$src, i8:$amt))]>;
def SRL8r : Pseudo<(outs GPR8:$dst), (ins GPR8:$src, GPR8:$amt),
                   "# SRL8 $dst, $src, $amt",
                   [(set i8:$dst, (srl i8:$src, i8:$amt))]>;
def SRA8r : Pseudo<(outs GPR8:$dst), (ins GPR8:$src, GPR8:$amt),
                   "# SRA8 $dst, $src, $amt",
                   [(set i8:$dst, (sra i8:$src, i8:$amt))]>;
}

// i16 ADD pseudo — expanded to copy to HL + ADD HL,rr.
let Defs = [F] in
def ADD16rr : Pseudo<(outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                     "# ADD16 $dst, $src1, $src2",
                     [(set i16:$dst, (add i16:$src1, i16:$src2))]>;

// i16 SUB pseudo — expanded to lo/hi byte subtraction.
let Defs = [F] in
def SUB16rr : Pseudo<(outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                     "# SUB16 $dst, $src1, $src2",
                     [(set i16:$dst, (sub i16:$src1, i16:$src2))]>;

// i16 bitwise pseudos — lo/hi byte decomposition.
let Defs = [F] in {
def AND16rr : Pseudo<(outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                     "# AND16 $dst, $src1, $src2",
                     [(set i16:$dst, (and i16:$src1, i16:$src2))]>;
def OR16rr : Pseudo<(outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                    "# OR16 $dst, $src1, $src2",
                    [(set i16:$dst, (or i16:$src1, i16:$src2))]>;
def XOR16rr : Pseudo<(outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                     "# XOR16 $dst, $src1, $src2",
                     [(set i16:$dst, (xor i16:$src1, i16:$src2))]>;
}

// i16 shift left pseudo — SLA lo, RL hi (shift by 1).
let Defs = [F] in
def SHL16rr : Pseudo<(outs GR16:$dst), (ins GR16:$src, GPR8:$amt),
                     "# SHL16 $dst, $src, $amt",
                     [(set i16:$dst, (shl i16:$src, (i8 GPR8:$amt)))]>;

// i16 load/store from global address.
let mayLoad = 1, hasSideEffects = 0 in
def LOAD16 : Pseudo<(outs GR16:$dst), (ins i16imm:$addr),
                    "# LOAD16 $dst, [$addr]",
                    [(set i16:$dst, (load (SM83Wrapper tglobaladdr:$addr)))]>;

let mayStore = 1, hasSideEffects = 0 in
def STORE16 : Pseudo<(outs), (ins GR16:$src, i16imm:$addr),
                     "# STORE16 [$addr], $src",
                     [(store i16:$src, (SM83Wrapper tglobaladdr:$addr))]>;

//===----------------------------------------------------------------------===//
// NOP / HALT / STOP / DI / EI / DAA / CPL / SCF / CCF
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def NOP : SM83Inst8<(outs), (ins), "nop", []>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
def HALT : SM83Inst8<(outs), (ins), "halt", []>;
def STOP : SM83Inst16<(outs), (ins), "stop", []>;
def DI   : SM83Inst8<(outs), (ins), "di", []>;
def EI   : SM83Inst8<(outs), (ins), "ei", []>;
}

let Defs = [A, F], Uses = [A, F], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def DAA : SM83Inst8<(outs), (ins), "daa", []>;

let Defs = [A, F], Uses = [A], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def CPL : SM83Inst8<(outs), (ins), "cpl", []>;

let Defs = [F], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def SCF : SM83Inst8<(outs), (ins), "scf", []>;
def CCF : SM83Inst8<(outs), (ins), "ccf", []>;
}

//===----------------------------------------------------------------------===//
// 8-bit Load instructions: LD r, r'
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def LDrr : SM83Inst8<(outs GPR8:$dst), (ins GPR8:$src),
                     "ld\t$dst, $src", []>;

//===----------------------------------------------------------------------===//
// 8-bit Load immediate: LD r, imm8
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isReMaterializable = 1 in
def LDri : SM83Inst16<(outs GPR8:$dst), (ins i8imm:$imm),
                      "ld\t$dst, $imm",
                      [(set i8:$dst, imm:$imm)]>;

//===----------------------------------------------------------------------===//
// 8-bit Load from (HL): LD r, (HL)
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 0, mayStore = 0, Uses = [HL] in
def LDrHL : SM83Inst8<(outs GPR8:$dst), (ins),
                      "ld\t$dst, [hl]", []>;

//===----------------------------------------------------------------------===//
// 8-bit Store to (HL): LD (HL), r
//===----------------------------------------------------------------------===//

let mayStore = 1, hasSideEffects = 0, mayLoad = 0, Uses = [HL] in
def LDHLr : SM83Inst8<(outs), (ins GPR8:$src),
                      "ld\t[hl], $src", []>;

//===----------------------------------------------------------------------===//
// 8-bit Store immediate to (HL): LD (HL), imm8
//===----------------------------------------------------------------------===//

let mayStore = 1, hasSideEffects = 0, mayLoad = 0, Uses = [HL] in
def LDHLi : SM83Inst16<(outs), (ins i8imm:$imm),
                       "ld\t[hl], $imm", []>;

//===----------------------------------------------------------------------===//
// 8-bit Indirect loads through A
//===----------------------------------------------------------------------===//

let Defs = [A], mayLoad = 1, hasSideEffects = 0, mayStore = 0 in {
def LDA_BC : SM83Inst8<(outs), (ins), "ld\ta, [bc]", []> {
  let Uses = [BC];
}
def LDA_DE : SM83Inst8<(outs), (ins), "ld\ta, [de]", []> {
  let Uses = [DE];
}
def LDA_HLI : SM83Inst8<(outs), (ins), "ld\ta, [hl+]", []> {
  let Uses = [HL];
  let Defs = [A, HL];
}
def LDA_HLD : SM83Inst8<(outs), (ins), "ld\ta, [hl-]", []> {
  let Uses = [HL];
  let Defs = [A, HL];
}
}

let Uses = [A], mayStore = 1, hasSideEffects = 0, mayLoad = 0 in {
def LDBC_A : SM83Inst8<(outs), (ins), "ld\t[bc], a", []> {
  let Uses = [A, BC];
}
def LDDE_A : SM83Inst8<(outs), (ins), "ld\t[de], a", []> {
  let Uses = [A, DE];
}
def LDHLI_A : SM83Inst8<(outs), (ins), "ld\t[hl+], a", []> {
  let Uses = [A, HL];
  let Defs = [HL];
}
def LDHLD_A : SM83Inst8<(outs), (ins), "ld\t[hl-], a", []> {
  let Uses = [A, HL];
  let Defs = [HL];
}
}

//===----------------------------------------------------------------------===//
// Direct memory loads/stores: LD A,(imm16) / LD (imm16),A
//===----------------------------------------------------------------------===//

let Defs = [A], mayLoad = 1, hasSideEffects = 0, mayStore = 0 in
def LDA_nn : SM83Inst24<(outs), (ins i16imm:$addr),
                        "ld\ta, [$addr]", []>;

let Uses = [A], mayStore = 1, hasSideEffects = 0, mayLoad = 0 in
def LDnn_A : SM83Inst24<(outs), (ins i16imm:$addr),
                        "ld\t[$addr], a", []>;

//===----------------------------------------------------------------------===//
// High page: LDH A,(imm8) / LDH (imm8),A / LDH A,(C) / LDH (C),A
//===----------------------------------------------------------------------===//

let Defs = [A], mayLoad = 1, hasSideEffects = 0, mayStore = 0 in {
def LDH_An : SM83Inst16<(outs), (ins i8imm:$addr),
                        "ldh\ta, [$addr]", []>;
def LDH_AC : SM83Inst8<(outs), (ins), "ldh\ta, [c]", []> {
  let Uses = [C];
}
}

let Uses = [A], mayStore = 1, hasSideEffects = 0, mayLoad = 0 in {
def LDH_nA : SM83Inst16<(outs), (ins i8imm:$addr),
                        "ldh\t[$addr], a", []>;
def LDH_CA : SM83Inst8<(outs), (ins), "ldh\t[c], a", []> {
  let Uses = [A, C];
}
}

//===----------------------------------------------------------------------===//
// 16-bit Load immediate: LD rr, imm16
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isReMaterializable = 1 in
def LDrri : SM83Inst24<(outs GR16:$dst), (ins i16imm:$imm),
                       "ld\t$dst, $imm",
                       [(set i16:$dst, imm:$imm)]>;

//===----------------------------------------------------------------------===//
// LD SP, HL
//===----------------------------------------------------------------------===//

let Defs = [SP], Uses = [HL], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def LDSPHL : SM83Inst8<(outs), (ins), "ld\tsp, hl", []>;

//===----------------------------------------------------------------------===//
// LD (imm16), SP
//===----------------------------------------------------------------------===//

let Uses = [SP], mayStore = 1, hasSideEffects = 0, mayLoad = 0 in
def LDnn_SP : SM83Inst24<(outs), (ins i16imm:$addr),
                         "ld\t[$addr], sp", []>;

//===----------------------------------------------------------------------===//
// LD HL, SP+imm8s
//===----------------------------------------------------------------------===//

let Defs = [HL, F], Uses = [SP], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def LDHLSP : SM83Inst16<(outs), (ins i8imm:$offset),
                        "ld\thl, sp + $offset", []>;

//===----------------------------------------------------------------------===//
// PUSH / POP (16-bit pairs)
//===----------------------------------------------------------------------===//

let Defs = [SP], Uses = [SP], mayStore = 1, hasSideEffects = 0 in
def PUSH : SM83Inst8<(outs), (ins GPR16:$src),
                     "push\t$src", []>;

let Defs = [SP], Uses = [SP], mayLoad = 1, hasSideEffects = 0 in
def POP : SM83Inst8<(outs GPR16:$dst), (ins),
                    "pop\t$dst", []>;

//===----------------------------------------------------------------------===//
// ALU operations (8-bit, through accumulator A)
// ADD A,r / ADC A,r / SUB r / SBC A,r / AND r / OR r / XOR r / CP r
//===----------------------------------------------------------------------===//

let Defs = [A, F], Uses = [A], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

def ADDr : SM83Inst8<(outs), (ins GPR8:$src),
                     "add\ta, $src", []>;

def ADCr : SM83Inst8<(outs), (ins GPR8:$src),
                     "adc\ta, $src", []>;

def SUBr : SM83Inst8<(outs), (ins GPR8:$src),
                     "sub\t$src", []>;

def SBCr : SM83Inst8<(outs), (ins GPR8:$src),
                     "sbc\ta, $src", []>;

def ANDr : SM83Inst8<(outs), (ins GPR8:$src),
                     "and\t$src", []>;

def ORr : SM83Inst8<(outs), (ins GPR8:$src),
                    "or\t$src", []>;

def XORr : SM83Inst8<(outs), (ins GPR8:$src),
                     "xor\t$src", []>;

def CPr : SM83Inst8<(outs), (ins GPR8:$src),
                    "cp\t$src", []>;
}

//===----------------------------------------------------------------------===//
// ALU with immediate: ADD A,imm8 / ADC A,imm8 / SUB imm8 etc.
//===----------------------------------------------------------------------===//

let Defs = [A, F], Uses = [A], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

def ADDi : SM83Inst16<(outs), (ins i8imm:$imm),
                      "add\ta, $imm", []>;

def ADCi : SM83Inst16<(outs), (ins i8imm:$imm),
                      "adc\ta, $imm", []>;

def SUBi : SM83Inst16<(outs), (ins i8imm:$imm),
                      "sub\t$imm", []>;

def SBCi : SM83Inst16<(outs), (ins i8imm:$imm),
                      "sbc\ta, $imm", []>;

def ANDi : SM83Inst16<(outs), (ins i8imm:$imm),
                      "and\t$imm", []>;

def ORi : SM83Inst16<(outs), (ins i8imm:$imm),
                     "or\t$imm", []>;

def XORi : SM83Inst16<(outs), (ins i8imm:$imm),
                      "xor\t$imm", []>;

def CPi : SM83Inst16<(outs), (ins i8imm:$imm),
                     "cp\t$imm", []>;
}

//===----------------------------------------------------------------------===//
// ALU with (HL): ADD A,(HL) / SUB (HL) etc.
//===----------------------------------------------------------------------===//

let Defs = [A, F], Uses = [A, HL], mayLoad = 1, hasSideEffects = 0, mayStore = 0 in {

def ADDm : SM83Inst8<(outs), (ins), "add\ta, [hl]", []>;
def ADCm : SM83Inst8<(outs), (ins), "adc\ta, [hl]", []>;
def SUBm : SM83Inst8<(outs), (ins), "sub\t[hl]", []>;
def SBCm : SM83Inst8<(outs), (ins), "sbc\ta, [hl]", []>;
def ANDm : SM83Inst8<(outs), (ins), "and\t[hl]", []>;
def ORm  : SM83Inst8<(outs), (ins), "or\t[hl]", []>;
def XORm : SM83Inst8<(outs), (ins), "xor\t[hl]", []>;
def CPm  : SM83Inst8<(outs), (ins), "cp\t[hl]", []>;
}

//===----------------------------------------------------------------------===//
// 16-bit ADD: ADD HL, rr
//===----------------------------------------------------------------------===//

let Defs = [HL, F], Uses = [HL], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def ADDHLrr : SM83Inst8<(outs), (ins GR16:$src),
                        "add\thl, $src", []>;

//===----------------------------------------------------------------------===//
// ADD SP, imm8s
//===----------------------------------------------------------------------===//

let Defs = [SP, F], Uses = [SP], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def ADDSPi : SM83Inst16<(outs), (ins i8imm:$imm),
                        "add\tsp, $imm", []>;

//===----------------------------------------------------------------------===//
// INC / DEC (8-bit)
//===----------------------------------------------------------------------===//

let Defs = [F], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def INCr : SM83Inst8<(outs GPR8:$dst), (ins GPR8:$src),
                     "inc\t$dst",
                     []> {
  let Constraints = "$src = $dst";
}

def DECr : SM83Inst8<(outs GPR8:$dst), (ins GPR8:$src),
                     "dec\t$dst",
                     []> {
  let Constraints = "$src = $dst";
}
}

// INC/DEC (HL) — memory
let Defs = [F], Uses = [HL], mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
def INCm : SM83Inst8<(outs), (ins), "inc\t[hl]", []>;
def DECm : SM83Inst8<(outs), (ins), "dec\t[hl]", []>;
}

//===----------------------------------------------------------------------===//
// INC / DEC (16-bit)
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def INCrr : SM83Inst8<(outs GR16:$dst), (ins GR16:$src),
                      "inc\t$dst", []> {
  let Constraints = "$src = $dst";
}

def DECrr : SM83Inst8<(outs GR16:$dst), (ins GR16:$src),
                      "dec\t$dst", []> {
  let Constraints = "$src = $dst";
}
}

let Defs = [SP], Uses = [SP], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def INCSP : SM83Inst8<(outs), (ins), "inc\tsp", []>;
def DECSP : SM83Inst8<(outs), (ins), "dec\tsp", []>;
}

//===----------------------------------------------------------------------===//
// Rotate A instructions (no CB prefix, only affect carry)
//===----------------------------------------------------------------------===//

let Defs = [A, F], Uses = [A], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def RLCA : SM83Inst8<(outs), (ins), "rlca", []>;
def RRCA : SM83Inst8<(outs), (ins), "rrca", []>;
def RLA  : SM83Inst8<(outs), (ins), "rla", []>;
def RRA  : SM83Inst8<(outs), (ins), "rra", []>;
}

//===----------------------------------------------------------------------===//
// CB-prefix: Rotate/Shift on register
//===----------------------------------------------------------------------===//

let Defs = [F], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def RLCr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "rlc\t$dst", []> {
  let Constraints = "$src = $dst";
}
def RRCr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "rrc\t$dst", []> {
  let Constraints = "$src = $dst";
}
def RLr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "rl\t$dst", []> {
  let Constraints = "$src = $dst";
}
def RRr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "rr\t$dst", []> {
  let Constraints = "$src = $dst";
}
def SLAr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "sla\t$dst", []> {
  let Constraints = "$src = $dst";
}
def SRAr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "sra\t$dst", []> {
  let Constraints = "$src = $dst";
}
def SRLr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "srl\t$dst", []> {
  let Constraints = "$src = $dst";
}
def SWAPr : SM83Inst16<(outs GPR8:$dst), (ins GPR8:$src), "swap\t$dst", []> {
  let Constraints = "$src = $dst";
}
}

//===----------------------------------------------------------------------===//
// CB-prefix: Rotate/Shift on (HL)
//===----------------------------------------------------------------------===//

let Defs = [F], Uses = [HL], mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
def RLCm  : SM83Inst16<(outs), (ins), "rlc\t[hl]", []>;
def RRCm  : SM83Inst16<(outs), (ins), "rrc\t[hl]", []>;
def RLm   : SM83Inst16<(outs), (ins), "rl\t[hl]", []>;
def RRm   : SM83Inst16<(outs), (ins), "rr\t[hl]", []>;
def SLAm  : SM83Inst16<(outs), (ins), "sla\t[hl]", []>;
def SRAm  : SM83Inst16<(outs), (ins), "sra\t[hl]", []>;
def SRLm  : SM83Inst16<(outs), (ins), "srl\t[hl]", []>;
def SWAPm : SM83Inst16<(outs), (ins), "swap\t[hl]", []>;
}

//===----------------------------------------------------------------------===//
// CB-prefix: BIT b, r — test bit
//===----------------------------------------------------------------------===//

let Defs = [F], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def BITr : SM83Inst16<(outs), (ins i8imm:$bit, GPR8:$src),
                      "bit\t$bit, $src", []>;

let Defs = [F], Uses = [HL], mayLoad = 1, hasSideEffects = 0, mayStore = 0 in
def BITm : SM83Inst16<(outs), (ins i8imm:$bit),
                      "bit\t$bit, [hl]", []>;

//===----------------------------------------------------------------------===//
// CB-prefix: SET b, r / RES b, r
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
def SETr : SM83Inst16<(outs GPR8:$dst), (ins i8imm:$bit, GPR8:$src),
                      "set\t$bit, $dst", []> {
  let Constraints = "$src = $dst";
}
def RESr : SM83Inst16<(outs GPR8:$dst), (ins i8imm:$bit, GPR8:$src),
                      "res\t$bit, $dst", []> {
  let Constraints = "$src = $dst";
}
}

let Uses = [HL], mayLoad = 1, mayStore = 1, hasSideEffects = 0 in {
def SETm : SM83Inst16<(outs), (ins i8imm:$bit), "set\t$bit, [hl]", []>;
def RESm : SM83Inst16<(outs), (ins i8imm:$bit), "res\t$bit, [hl]", []>;
}

//===----------------------------------------------------------------------===//
// Control flow: JP / JR / CALL / RET / RST
//===----------------------------------------------------------------------===//

// Unconditional jumps
let isBranch = 1, isTerminator = 1, isBarrier = 1, hasSideEffects = 0 in {
def JP : SM83Inst24<(outs), (ins brtarget:$target),
                    "jp\t$target",
                    [(br bb:$target)]>;

def JR : SM83Inst16<(outs), (ins brtarget:$target),
                    "jr\t$target", []>;
}

// Conditional jumps
let isBranch = 1, isTerminator = 1, hasSideEffects = 0, Uses = [F] in {
def JPcc : SM83Inst24<(outs), (ins i8imm:$cc, i16imm:$target),
                      "jp\t$cc, $target", []>;

def JRcc : SM83Inst16<(outs), (ins i8imm:$cc, brtarget:$target),
                      "jr\t$cc, $target", []>;
}

// Indirect jump: JP (HL)
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    Uses = [HL], hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def JPHL : SM83Inst8<(outs), (ins), "jp\thl", []>;

// CALL
let isCall = 1, Defs = [SP], Uses = [SP], hasSideEffects = 0 in {
def CALL : SM83Inst24<(outs), (ins i16imm:$target, variable_ops),
                      "call\t$target",
                      [(SM83call imm:$target)]>;

def CALLcc : SM83Inst24<(outs), (ins i8imm:$cc, i16imm:$target),
                        "call\t$cc, $target", []> {
  let Uses = [SP, F];
}
}

// Call patterns for global address and external symbol targets.
def : Pat<(SM83call (i16 tglobaladdr:$dst)), (CALL tglobaladdr:$dst)>;
def : Pat<(SM83call (i16 texternalsym:$dst)), (CALL texternalsym:$dst)>;

// RET
let isReturn = 1, isTerminator = 1, isBarrier = 1,
    Defs = [SP], Uses = [SP], hasSideEffects = 0, mayLoad = 1 in {
def RET : SM83Inst8<(outs), (ins), "ret",
                    [(SM83ret)]>;

def RETI : SM83Inst8<(outs), (ins), "reti", []>;
}

// Conditional RET
let isReturn = 1, isTerminator = 1, Defs = [SP], Uses = [SP, F],
    hasSideEffects = 0, mayLoad = 1 in
def RETcc : SM83Inst8<(outs), (ins i8imm:$cc), "ret\t$cc", []>;

// RST — restart vectors
let isCall = 1, Defs = [SP], Uses = [SP], hasSideEffects = 0 in
def RST : SM83Inst8<(outs), (ins i8imm:$vec), "rst\t$vec", []>;
