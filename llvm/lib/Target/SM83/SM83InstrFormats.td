//===-- SM83InstrFormats.td - SM83 Instruction Formats ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SM83 Instruction Format Definitions.
//
//===----------------------------------------------------------------------===//

// A generic SM83 instruction.
class SM83Inst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  let Namespace = "SM83";

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// 1-byte SM83 instruction.
class SM83Inst8<dag outs, dag ins, string asmstr, list<dag> pattern>
    : SM83Inst<outs, ins, asmstr, pattern> {
  field bits<8> Inst;
  let Size = 1;
}

// 2-byte SM83 instruction (opcode + immediate8 or CB prefix + opcode).
class SM83Inst16<dag outs, dag ins, string asmstr, list<dag> pattern>
    : SM83Inst<outs, ins, asmstr, pattern> {
  field bits<16> Inst;
  let Size = 2;
}

// 3-byte SM83 instruction (opcode + immediate16).
class SM83Inst24<dag outs, dag ins, string asmstr, list<dag> pattern>
    : SM83Inst<outs, ins, asmstr, pattern> {
  field bits<24> Inst;
  let Size = 3;
}

// Pseudo instruction — not a real SM83 instruction.
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : SM83Inst<outs, ins, asmstr, pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
