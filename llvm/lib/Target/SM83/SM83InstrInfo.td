//===-- SM83InstrInfo.td - SM83 Instruction defs ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the SM83 instructions in TableGen format.
// Phase 1: minimal definitions to get the backend to compile.
// Full instruction set will be added in Phase 2.
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

//===----------------------------------------------------------------------===//
// NOP
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def NOP : SM83Inst8<(outs), (ins), "nop", []>;
