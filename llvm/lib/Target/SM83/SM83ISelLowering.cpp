//===-- SM83ISelLowering.cpp - SM83 DAG Lowering Implementation -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SM83ISelLowering.h"

#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

#include "SM83.h"
#include "SM83MachineFunctionInfo.h"
#include "SM83Subtarget.h"
#include "SM83TargetMachine.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

namespace llvm {

#include "SM83GenCallingConv.inc"

SM83TargetLowering::SM83TargetLowering(const SM83TargetMachine &TM,
                                       const SM83Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  addRegisterClass(MVT::i8, &SM83::GPR8RegClass);
  addRegisterClass(MVT::i16, &SM83::GR16RegClass);

  computeRegisterProperties(Subtarget.getRegisterInfo());

  setBooleanContents(ZeroOrOneBooleanContent);
  setSchedulingPreference(Sched::RegPressure);
  setStackPointerRegisterToSaveRestore(SM83::SP);

  // Load/store extension actions (like AVR and MSP430).
  for (MVT VT : {MVT::i8, MVT::i16}) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
  }
  setLoadExtAction(ISD::SEXTLOAD, MVT::i16, MVT::i8, Expand);
  setLoadExtAction(ISD::ZEXTLOAD, MVT::i16, MVT::i8, Expand);
  setLoadExtAction(ISD::EXTLOAD, MVT::i16, MVT::i8, Expand);
  setTruncStoreAction(MVT::i16, MVT::i8, Expand);

  // Custom lowering.
  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i16, Custom);
  // FrameIndex must be Custom so taking the address of a local variable works.
  // Lowered to LDHLSP (LD HL, SP+offset) in ISelDAGToDAG.
  setOperationAction(ISD::BR_CC, MVT::i8, Custom);
  setOperationAction(ISD::BR_CC, MVT::i16, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i8, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i16, Custom);
  setOperationAction(ISD::SETCC, MVT::i8, Custom);
  setOperationAction(ISD::SETCC, MVT::i16, Custom);
  setOperationAction(ISD::SELECT, MVT::i8, Expand);
  setOperationAction(ISD::SELECT, MVT::i16, Expand);

  // Raw BRCOND must be expanded to BR_CC (which we handle via Custom lowering).
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  // Jump tables: expand to if-else chains (SM83 has no indexed jump).
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setMinimumJumpTableEntries(INT_MAX); // Disable jump table generation entirely.

  // i32 comparison/branching — Custom so we can decompose into chained i16 compares.
  setOperationAction(ISD::BR_CC, MVT::i32, Custom);
  setOperationAction(ISD::SETCC, MVT::i32, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
  setOperationAction(ISD::SELECT, MVT::i32, Expand);

  // i64 comparison/branching.
  setOperationAction(ISD::BR_CC, MVT::i64, Custom);
  setOperationAction(ISD::SETCC, MVT::i64, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i64, Expand);
  setOperationAction(ISD::SELECT, MVT::i64, Expand);

  // Expand operations SM83 can't do natively.
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  // SM83 has no hardware support for variable-length stack allocation (VLAs).
  // Mark as Custom so we can emit a fatal error rather than silently miscompile.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i8, Custom);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Custom);

  setOperationAction(ISD::MUL, MVT::i8, Custom);
  setOperationAction(ISD::MUL, MVT::i16, Expand);
  setOperationAction(ISD::SDIV, MVT::i8, Expand);
  setOperationAction(ISD::SDIV, MVT::i16, Expand);
  setOperationAction(ISD::UDIV, MVT::i8, Expand);
  setOperationAction(ISD::UDIV, MVT::i16, Expand);
  setOperationAction(ISD::SREM, MVT::i8, Expand);
  setOperationAction(ISD::SREM, MVT::i16, Expand);
  setOperationAction(ISD::UREM, MVT::i8, Expand);
  setOperationAction(ISD::UREM, MVT::i16, Expand);
  // DAG combiner merges UDIV+UREM into UDIVREM; must Expand or it falls through.
  setOperationAction(ISD::UDIVREM, MVT::i8, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i8, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i16, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i16, Expand);
  setOperationAction(ISD::MULHS, MVT::i8, Expand);
  setOperationAction(ISD::MULHU, MVT::i8, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i8, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i8, Expand);
  setOperationAction(ISD::MULHS, MVT::i16, Expand);
  setOperationAction(ISD::MULHU, MVT::i16, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i16, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i16, Expand);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
  setOperationAction(ISD::ROTL, MVT::i8, Expand);
  setOperationAction(ISD::ROTR, MVT::i8, Expand);
  setOperationAction(ISD::ROTL, MVT::i16, Expand);
  setOperationAction(ISD::ROTR, MVT::i16, Expand);
  setOperationAction(ISD::CTPOP, MVT::i8, Expand);
  setOperationAction(ISD::CTLZ, MVT::i8, Expand);
  setOperationAction(ISD::CTTZ, MVT::i8, Expand);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i8, Expand);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i8, Expand);
  setOperationAction(ISD::CTPOP, MVT::i16, Expand);
  setOperationAction(ISD::CTLZ, MVT::i16, Expand);
  setOperationAction(ISD::CTTZ, MVT::i16, Expand);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i16, Expand);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i16, Expand);
  setOperationAction(ISD::BSWAP, MVT::i16, Expand);
  setOperationAction(ISD::ABS, MVT::i8, Expand);
  setOperationAction(ISD::ABS, MVT::i16, Expand);
  setOperationAction(ISD::SMIN, MVT::i8, Expand);
  setOperationAction(ISD::SMAX, MVT::i8, Expand);
  setOperationAction(ISD::UMIN, MVT::i8, Expand);
  setOperationAction(ISD::UMAX, MVT::i8, Expand);
  setOperationAction(ISD::SMIN, MVT::i16, Expand);
  setOperationAction(ISD::SMAX, MVT::i16, Expand);
  setOperationAction(ISD::UMIN, MVT::i16, Expand);
  setOperationAction(ISD::UMAX, MVT::i16, Expand);

  setOperationAction(ISD::SHL_PARTS, MVT::i8, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i8, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i8, Expand);
  setOperationAction(ISD::SHL_PARTS, MVT::i16, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i16, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i16, Expand);

  // SM83 has no varargs support.
  setOperationAction(ISD::VAARG,  MVT::Other, Expand);
  setOperationAction(ISD::VAEND,  MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);

  // i32/i64 arithmetic — LLVM decomposes to i16 pairs via libcalls.
  for (auto VT : {MVT::i32, MVT::i64}) {
    setOperationAction(ISD::MUL, VT, Expand);
    setOperationAction(ISD::SDIV, VT, Expand);
    setOperationAction(ISD::UDIV, VT, Expand);
    setOperationAction(ISD::SREM, VT, Expand);
    setOperationAction(ISD::UREM, VT, Expand);
    setOperationAction(ISD::UDIVREM, VT, Expand);
    setOperationAction(ISD::SDIVREM, VT, Expand);
    setOperationAction(ISD::MULHS, VT, Expand);
    setOperationAction(ISD::MULHU, VT, Expand);
    setOperationAction(ISD::SMUL_LOHI, VT, Expand);
    setOperationAction(ISD::UMUL_LOHI, VT, Expand);
    setOperationAction(ISD::SHL, VT, Expand);
    setOperationAction(ISD::SRL, VT, Expand);
    setOperationAction(ISD::SRA, VT, Expand);
    setOperationAction(ISD::ROTL, VT, Expand);
    setOperationAction(ISD::ROTR, VT, Expand);
    setOperationAction(ISD::CTPOP, VT, Expand);
    setOperationAction(ISD::CTLZ, VT, Expand);
    setOperationAction(ISD::CTTZ, VT, Expand);
    setOperationAction(ISD::CTLZ_ZERO_UNDEF, VT, Expand);
    setOperationAction(ISD::CTTZ_ZERO_UNDEF, VT, Expand);
    setOperationAction(ISD::BSWAP, VT, Expand);
    setOperationAction(ISD::ABS,   VT, Expand);
    setOperationAction(ISD::SMIN,  VT, Expand);
    setOperationAction(ISD::SMAX,  VT, Expand);
    setOperationAction(ISD::UMIN,  VT, Expand);
    setOperationAction(ISD::UMAX,  VT, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT, Expand);
  }

  // SM83 has no FPU. LLVM auto-softens FP types since no FP register class
  // is registered, but we must mark FP operations explicitly to prevent
  // the legalizer from generating unsupported libcalls that crash.
  for (MVT VT : {MVT::f32, MVT::f64, MVT::f128}) {
    setOperationAction(ISD::FADD, VT, Expand);
    setOperationAction(ISD::FSUB, VT, Expand);
    setOperationAction(ISD::FMUL, VT, Expand);
    setOperationAction(ISD::FDIV, VT, Expand);
    setOperationAction(ISD::FREM, VT, Expand);
    setOperationAction(ISD::FNEG, VT, Expand);
    setOperationAction(ISD::FABS, VT, Expand);
    setOperationAction(ISD::FSQRT, VT, Expand);
    setOperationAction(ISD::FSIN, VT, Expand);
    setOperationAction(ISD::FCOS, VT, Expand);
    setOperationAction(ISD::FPOW, VT, Expand);
    setOperationAction(ISD::FLOG, VT, Expand);
    setOperationAction(ISD::FLOG2, VT, Expand);
    setOperationAction(ISD::FLOG10, VT, Expand);
    setOperationAction(ISD::FEXP, VT, Expand);
    setOperationAction(ISD::FEXP2, VT, Expand);
    setOperationAction(ISD::FCEIL, VT, Expand);
    setOperationAction(ISD::FFLOOR, VT, Expand);
    setOperationAction(ISD::FROUND, VT, Expand);
    setOperationAction(ISD::FTRUNC, VT, Expand);
    setOperationAction(ISD::FRINT, VT, Expand);
    setOperationAction(ISD::FNEARBYINT, VT, Expand);
    setOperationAction(ISD::FP_TO_SINT, VT, Expand);
    setOperationAction(ISD::FP_TO_UINT, VT, Expand);
    setOperationAction(ISD::SINT_TO_FP, VT, Expand);
    setOperationAction(ISD::UINT_TO_FP, VT, Expand);
    setOperationAction(ISD::FP_ROUND, VT, Expand);
    setOperationAction(ISD::FP_EXTEND, VT, Expand);
    setOperationAction(ISD::FCOPYSIGN, VT, Expand);
    setOperationAction(ISD::FMA, VT, Expand);
    setOperationAction(ISD::FMINNUM, VT, Expand);
    setOperationAction(ISD::FMAXNUM, VT, Expand);
    setOperationAction(ISD::BR_CC, VT, Expand);
    setOperationAction(ISD::SETCC, VT, Expand);
    setOperationAction(ISD::SELECT, VT, Expand);
    setOperationAction(ISD::SELECT_CC, VT, Expand);
  }

  // SM83 is single-threaded: atomic loads/stores are just plain loads/stores.
  // Custom-lower them to strip the atomic ordering and emit regular operations.
  for (MVT VT : {MVT::i8, MVT::i16}) {
    setOperationAction(ISD::ATOMIC_LOAD, VT, Custom);
    setOperationAction(ISD::ATOMIC_STORE, VT, Custom);
    // Atomic RMW operations — expand to load/op/store sequences.
    setOperationAction(ISD::ATOMIC_LOAD_ADD, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_SUB, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_AND, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_OR, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_XOR, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_NAND, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_MIN, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_MAX, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_UMIN, VT, Expand);
    setOperationAction(ISD::ATOMIC_LOAD_UMAX, VT, Expand);
    setOperationAction(ISD::ATOMIC_SWAP, VT, Expand);
    setOperationAction(ISD::ATOMIC_CMP_SWAP, VT, Expand);
  }
  // Fences are no-ops on single-threaded SM83.
  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);

  setMinFunctionAlignment(Align(1));
}

SDValue SM83TargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::ExternalSymbol:
    return LowerExternalSymbol(Op, DAG);
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:
    return LowerSELECT_CC(Op, DAG);
  case ISD::SETCC:
    return LowerSETCC(Op, DAG);
  case ISD::MUL:
    return LowerMUL(Op, DAG);
  case ISD::ATOMIC_LOAD:
    return LowerATOMIC_LOAD(Op, DAG);
  case ISD::ATOMIC_STORE:
    return LowerATOMIC_STORE(Op, DAG);
  case ISD::ATOMIC_FENCE:
    // Single-threaded: fences are no-ops. Just return the chain.
    return Op.getOperand(0);
  case ISD::DYNAMIC_STACKALLOC:
    report_fatal_error("SM83 does not support variable-length arrays (VLAs) "
                       "or dynamic stack allocation");
  default:
    report_fatal_error("SM83: unimplemented operand");
  }
}

SDValue SM83TargetLowering::LowerATOMIC_LOAD(SDValue Op,
                                             SelectionDAG &DAG) const {
  // SM83 is single-threaded: atomic load → plain load.
  auto *AN = cast<AtomicSDNode>(Op.getNode());
  SDLoc DL(Op);
  return DAG.getLoad(AN->getMemoryVT(), DL, AN->getChain(),
                     AN->getBasePtr(), AN->getMemOperand());
}

SDValue SM83TargetLowering::LowerATOMIC_STORE(SDValue Op,
                                              SelectionDAG &DAG) const {
  // SM83 is single-threaded: atomic store → plain store.
  auto *AN = cast<AtomicSDNode>(Op.getNode());
  SDLoc DL(Op);
  return DAG.getStore(AN->getChain(), DL, AN->getOperand(1),
                      AN->getBasePtr(), AN->getMemOperand());
}

SDValue SM83TargetLowering::LowerGlobalAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  const GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op);
  SDLoc DL(Op);
  EVT VT = Op.getValueType();

  const GlobalValue *GV = GA->getGlobal();
  SDValue Result = DAG.getTargetGlobalAddress(GV, DL, VT, GA->getOffset());
  return DAG.getNode(SM83ISD::WRAPPER, DL, VT, Result);
}

SDValue SM83TargetLowering::LowerExternalSymbol(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  SDValue Result = DAG.getTargetExternalSymbol(Sym, VT);
  return DAG.getNode(SM83ISD::WRAPPER, DL, VT, Result);
}

SDValue SM83TargetLowering::LowerMUL(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);

  // Lower i8 multiply to a call to __mulqi3(i8, i8) -> i8.
  Type *I8Ty = Type::getInt8Ty(*DAG.getContext());
  TargetLowering::ArgListTy Args;
  Args.push_back(TargetLowering::ArgListEntry(LHS, I8Ty));
  Args.push_back(TargetLowering::ArgListEntry(RHS, I8Ty));

  SDValue Callee = DAG.getExternalSymbol("__mulqi3", MVT::i16);
  TargetLowering::CallLoweringInfo CLI(DAG);
  CLI.setDebugLoc(DL).setChain(DAG.getEntryNode()).setCallee(
      CallingConv::C, Type::getInt8Ty(*DAG.getContext()), Callee,
      std::move(Args));

  std::pair<SDValue, SDValue> CallResult = LowerCallTo(CLI);
  return CallResult.first;
}

// Map ISD condition code to SM83 condition code (NZ=0, Z=1, NC=2, C=3).
// May swap LHS/RHS to handle ugt/ule/sgt/sle by converting to ult/uge/slt/sge.
// For signed comparisons, we XOR both operands with 0x80 to convert to unsigned.
static unsigned mapCondCode(ISD::CondCode CC, SDValue &LHS, SDValue &RHS,
                            SelectionDAG &DAG, const SDLoc &DL,
                            bool &NeedSignFlip) {
  NeedSignFlip = false;
  switch (CC) {
  case ISD::SETEQ:  return 1; // Z
  case ISD::SETNE:  return 0; // NZ
  case ISD::SETULT: return 3; // C (LHS < RHS unsigned)
  case ISD::SETUGE: return 2; // NC (LHS >= RHS unsigned)
  case ISD::SETUGT: // swap to ult
    std::swap(LHS, RHS);
    return 3; // C
  case ISD::SETULE: // swap to uge
    std::swap(LHS, RHS);
    return 2; // NC
  case ISD::SETLT:  // signed: flip sign bits, then use unsigned
    NeedSignFlip = true;
    return 3; // C
  case ISD::SETGE:
    NeedSignFlip = true;
    return 2; // NC
  case ISD::SETGT:
    NeedSignFlip = true;
    std::swap(LHS, RHS);
    return 3; // C
  case ISD::SETLE:
    NeedSignFlip = true;
    std::swap(LHS, RHS);
    return 2; // NC
  default:
    report_fatal_error("SM83: unsupported condition code");
  }
}

// Apply sign flip: XOR both operands with 0x80 to convert signed compare
// to unsigned. This works because flipping the sign bit maps the signed
// range [-128,127] to unsigned [0,255] preserving order.
static void applySignFlip(SDValue &LHS, SDValue &RHS, SelectionDAG &DAG,
                          const SDLoc &DL) {
  SDValue Flip = DAG.getConstant(0x80, DL, MVT::i8);
  LHS = DAG.getNode(ISD::XOR, DL, MVT::i8, LHS, Flip);
  RHS = DAG.getNode(ISD::XOR, DL, MVT::i8, RHS, Flip);
}

// Decompose an i32 or i64 value into i16 halves via EXTRACT_ELEMENT.
// For i32: returns 2 i16 values. For i64: returns 4 i16 values.
static SmallVector<SDValue, 4> decomposeToI16(SDValue V, SelectionDAG &DAG,
                                               const SDLoc &DL) {
  SmallVector<SDValue, 4> Parts;
  EVT VT = V.getValueType();
  if (VT == MVT::i32) {
    SDValue Lo = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, V,
                             DAG.getIntPtrConstant(0, DL));
    SDValue Hi = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, V,
                             DAG.getIntPtrConstant(1, DL));
    Parts.push_back(Lo);
    Parts.push_back(Hi);
  } else if (VT == MVT::i64) {
    SDValue Lo32 = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, V,
                               DAG.getIntPtrConstant(0, DL));
    SDValue Hi32 = DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i32, V,
                               DAG.getIntPtrConstant(1, DL));
    for (SDValue Half : {Lo32, Hi32}) {
      Parts.push_back(DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, Half,
                                  DAG.getIntPtrConstant(0, DL)));
      Parts.push_back(DAG.getNode(ISD::EXTRACT_ELEMENT, DL, MVT::i16, Half,
                                  DAG.getIntPtrConstant(1, DL)));
    }
  }
  return Parts;
}

SDValue SM83TargetLowering::emitCmp(SDValue LHS, SDValue RHS,
                                    ISD::CondCode CC, bool NeedSignFlip,
                                    SelectionDAG &DAG,
                                    const SDLoc &DL) const {
  EVT VT = LHS.getValueType();

  // i32 and i64 comparisons: decompose to i16 pairs.
  if (VT == MVT::i32 || VT == MVT::i64) {
    SmallVector<SDValue, 4> LParts = decomposeToI16(LHS, DAG, DL);
    SmallVector<SDValue, 4> RParts = decomposeToI16(RHS, DAG, DL);
    unsigned N = LParts.size();

    if (CC == ISD::SETEQ || CC == ISD::SETNE) {
      // Equality: XOR each i16 pair, OR all results, compare with zero.
      SDValue Acc = DAG.getNode(ISD::XOR, DL, MVT::i16, LParts[0], RParts[0]);
      for (unsigned i = 1; i < N; ++i) {
        SDValue X = DAG.getNode(ISD::XOR, DL, MVT::i16, LParts[i], RParts[i]);
        Acc = DAG.getNode(ISD::OR, DL, MVT::i16, Acc, X);
      }
      SDValue Zero = DAG.getConstant(0, DL, MVT::i16);
      return DAG.getNode(SM83ISD::CMP16EQ, DL, MVT::Glue, Acc, Zero);
    }

    // Ordered comparison: chain CMP16 on lo pair, then CMPC16 on remaining pairs.
    if (NeedSignFlip) {
      // Flip sign bit of the topmost i16 half only.
      SDValue Flip = DAG.getConstant(0x8000, DL, MVT::i16);
      LParts[N - 1] = DAG.getNode(ISD::XOR, DL, MVT::i16, LParts[N - 1], Flip);
      RParts[N - 1] = DAG.getNode(ISD::XOR, DL, MVT::i16, RParts[N - 1], Flip);
    }
    SDValue Cmp = DAG.getNode(SM83ISD::CMP16, DL, MVT::Glue,
                              LParts[0], RParts[0]);
    for (unsigned i = 1; i < N; ++i)
      Cmp = DAG.getNode(SM83ISD::CMPC16, DL, MVT::Glue,
                         LParts[i], RParts[i], Cmp);
    return Cmp;
  }

  if (LHS.getValueType() == MVT::i16) {
    if (CC == ISD::SETEQ || CC == ISD::SETNE)
      return DAG.getNode(SM83ISD::CMP16EQ, DL, MVT::Glue, LHS, RHS);
    if (NeedSignFlip) {
      SDValue Flip = DAG.getConstant(0x8000, DL, MVT::i16);
      LHS = DAG.getNode(ISD::XOR, DL, MVT::i16, LHS, Flip);
      RHS = DAG.getNode(ISD::XOR, DL, MVT::i16, RHS, Flip);
    }
    return DAG.getNode(SM83ISD::CMP16, DL, MVT::Glue, LHS, RHS);
  }
  if (NeedSignFlip)
    applySignFlip(LHS, RHS, DAG, DL);
  return DAG.getNode(SM83ISD::CMP, DL, MVT::Glue, LHS, RHS);
}

SDValue SM83TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);

  bool NeedSignFlip;
  unsigned SM83CC = mapCondCode(CC, LHS, RHS, DAG, DL, NeedSignFlip);
  SDValue Cmp = emitCmp(LHS, RHS, CC, NeedSignFlip, DAG, DL);
  SDValue CCVal = DAG.getConstant(SM83CC, DL, MVT::i8);
  return DAG.getNode(SM83ISD::BRCOND, DL, MVT::Other, Chain, Dest, CCVal, Cmp);
}

SDValue SM83TargetLowering::LowerSELECT_CC(SDValue Op,
                                            SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  SDValue TrueVal = Op.getOperand(2);
  SDValue FalseVal = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();

  bool NeedSignFlip;
  unsigned SM83CC = mapCondCode(CC, LHS, RHS, DAG, DL, NeedSignFlip);
  SDValue Cmp = emitCmp(LHS, RHS, CC, NeedSignFlip, DAG, DL);
  SDValue CCVal = DAG.getConstant(SM83CC, DL, MVT::i8);
  return DAG.getNode(SM83ISD::SELECT_CC, DL, Op.getValueType(),
                     TrueVal, FalseVal, CCVal, Cmp);
}

SDValue SM83TargetLowering::LowerSETCC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();

  bool NeedSignFlip;
  unsigned SM83CC = mapCondCode(CC, LHS, RHS, DAG, DL, NeedSignFlip);
  SDValue Cmp = emitCmp(LHS, RHS, CC, NeedSignFlip, DAG, DL);
  SDValue CCVal = DAG.getConstant(SM83CC, DL, MVT::i8);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i8);
  SDValue One = DAG.getConstant(1, DL, MVT::i8);
  return DAG.getNode(SM83ISD::SELECT_CC, DL, MVT::i8, One, Zero, CCVal, Cmp);
}

MachineBasicBlock *SM83TargetLowering::insertShift(MachineInstr &MI,
                                                   MachineBasicBlock *MBB) const {
  const TargetInstrInfo &TII = *Subtarget.getInstrInfo();
  MachineFunction *MF = MBB->getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  DebugLoc DL = MI.getDebugLoc();

  unsigned Opc = MI.getOpcode();
  bool Is16 = (Opc == SM83::SHL16rr || Opc == SM83::SRL16rr ||
               Opc == SM83::SRA16rr);

  Register DstReg = MI.getOperand(0).getReg();
  Register SrcReg = MI.getOperand(1).getReg();
  Register AmtReg = MI.getOperand(2).getReg();

  // --- Constant-shift fast path: detect LDri def and unroll ---
  // If the shift amount is a compile-time constant (materialized by LDri),
  // emit N individual shift-by-1 instructions instead of a loop.
  MachineInstr *AmtDef = MRI.getVRegDef(AmtReg);
  if (AmtDef && AmtDef->getOpcode() == SM83::LDri) {
    unsigned RawImm = static_cast<unsigned>(AmtDef->getOperand(1).getImm());
    // For i8, shifts >= 8 are LLVM IR poison so masking to 7 is fine.
    // For i16, valid shift counts are 0–15; mask to 4 bits.
    uint8_t N = Is16 ? (RawImm & 0xF) : (RawImm & 0x7);
    // For i16 shifts >= 8 the loop handles the byte-boundary case correctly;
    // skip unrolling and fall through to the loop path below.
    if (Is16 && N > 7) {
      if (N == 8 && Opc != SM83::SRA16rr) {
        // Byte-swap optimisation: shift-by-8 is just a register move + zero.
        // shl i16, 8 : result_hi = src_lo, result_lo = 0  (2 instructions)
        // lshr i16, 8: result_lo = src_hi, result_hi = 0  (2 instructions)
        Register CurLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
        Register CurHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
        BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), CurLo)
            .addReg(SrcReg, RegState::NoFlags, SM83::sub_lo);
        BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), CurHi)
            .addReg(SrcReg, RegState::NoFlags, SM83::sub_hi);

        Register NewLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
        Register NewHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
        if (Opc == SM83::SHL16rr) {
          BuildMI(*MBB, MI, DL, TII.get(SM83::LDri), NewLo).addImm(0);
          BuildMI(*MBB, MI, DL, TII.get(SM83::LDrr), NewHi).addReg(CurLo);
        } else { // SRL16rr
          BuildMI(*MBB, MI, DL, TII.get(SM83::LDrr), NewLo).addReg(CurHi);
          BuildMI(*MBB, MI, DL, TII.get(SM83::LDri), NewHi).addImm(0);
        }
        BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::REG_SEQUENCE), DstReg)
            .addReg(NewLo).addImm(SM83::sub_lo)
            .addReg(NewHi).addImm(SM83::sub_hi);
        MI.eraseFromParent();
        return MBB;
      }
      goto use_loop;
    }

    if (Is16) {
      // Extract lo/hi from GR16 source.
      Register CurLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
      Register CurHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
      BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), CurLo)
          .addReg(SrcReg, RegState::NoFlags, SM83::sub_lo);
      BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), CurHi)
          .addReg(SrcReg, RegState::NoFlags, SM83::sub_hi);

      for (uint8_t i = 0; i < N; ++i) {
        Register NextLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
        Register NextHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
        if (Opc == SM83::SHL16rr) {
          BuildMI(*MBB, MI, DL, TII.get(SM83::SLAr), NextLo).addReg(CurLo);
          BuildMI(*MBB, MI, DL, TII.get(SM83::RLr),  NextHi).addReg(CurHi);
        } else if (Opc == SM83::SRL16rr) {
          BuildMI(*MBB, MI, DL, TII.get(SM83::SRLr), NextHi).addReg(CurHi);
          BuildMI(*MBB, MI, DL, TII.get(SM83::RRr),  NextLo).addReg(CurLo);
        } else { // SRA16rr
          BuildMI(*MBB, MI, DL, TII.get(SM83::SRAr), NextHi).addReg(CurHi);
          BuildMI(*MBB, MI, DL, TII.get(SM83::RRr),  NextLo).addReg(CurLo);
        }
        CurLo = NextLo;
        CurHi = NextHi;
      }

      // Recombine lo/hi into the GR16 destination.
      BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::REG_SEQUENCE), DstReg)
          .addReg(CurLo).addImm(SM83::sub_lo)
          .addReg(CurHi).addImm(SM83::sub_hi);
    } else {
      // 8-bit: determine hardware shift opcode.
      unsigned ShiftOpc;
      if (Opc == SM83::SHL8r)      ShiftOpc = SM83::SLAr;
      else if (Opc == SM83::SRL8r) ShiftOpc = SM83::SRLr;
      else                          ShiftOpc = SM83::SRAr;

      Register CurReg = SrcReg;
      for (uint8_t i = 0; i < N; ++i) {
        Register NextReg = (i == N - 1)
                               ? DstReg
                               : MRI.createVirtualRegister(&SM83::GPR8RegClass);
        BuildMI(*MBB, MI, DL, TII.get(ShiftOpc), NextReg).addReg(CurReg);
        CurReg = NextReg;
      }
      if (N == 0)
        BuildMI(*MBB, MI, DL, TII.get(SM83::LDrr), DstReg).addReg(SrcReg);
    }

    MI.eraseFromParent();
    return MBB;
  }
  // --- End constant-shift fast path ---

use_loop:
  // Create loop and done MBBs.
  MachineBasicBlock *LoopMBB = MF->CreateMachineBasicBlock(MBB->getBasicBlock());
  MachineBasicBlock *DoneMBB = MF->CreateMachineBasicBlock(MBB->getBasicBlock());

  MF->insert(++MBB->getIterator(), LoopMBB);
  MF->insert(++LoopMBB->getIterator(), DoneMBB);

  // Move everything after MI into DoneMBB.
  DoneMBB->splice(DoneMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());
  DoneMBB->transferSuccessorsAndUpdatePHIs(MBB);

  // Set up successors.
  MBB->addSuccessor(LoopMBB);
  MBB->addSuccessor(DoneMBB);
  LoopMBB->addSuccessor(LoopMBB); // back-edge
  LoopMBB->addSuccessor(DoneMBB);

  if (Is16) {
    // Extract lo/hi virtual registers from the GR16 source.
    Register SrcLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register SrcHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), SrcLo)
        .addReg(SrcReg, RegState::NoFlags, SM83::sub_lo);
    BuildMI(*MBB, MI, DL, TII.get(TargetOpcode::COPY), SrcHi)
        .addReg(SrcReg, RegState::NoFlags, SM83::sub_hi);

    // Zero-test: LD A, amt; OR A → Z=1 if amt==0.
    BuildMI(*MBB, MI, DL, TII.get(SM83::LDrr), SM83::A).addReg(AmtReg);
    BuildMI(*MBB, MI, DL, TII.get(SM83::ORr)).addReg(SM83::A);
    BuildMI(*MBB, MI, DL, TII.get(SM83::JRcc)).addImm(1).addMBB(DoneMBB);

    // Create loop vregs.
    Register ValLo  = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register ValHi  = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register CntReg = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register ShiftedLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register ShiftedHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register DecCnt    = MRI.createVirtualRegister(&SM83::GPR8RegClass);

    // LoopMBB PHIs (must be first instructions).
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::PHI), ValLo)
        .addReg(SrcLo).addMBB(MBB).addReg(ShiftedLo).addMBB(LoopMBB);
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::PHI), ValHi)
        .addReg(SrcHi).addMBB(MBB).addReg(ShiftedHi).addMBB(LoopMBB);
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::PHI), CntReg)
        .addReg(AmtReg).addMBB(MBB).addReg(DecCnt).addMBB(LoopMBB);

    // Shift body: direction-specific carry-propagating pair.
    if (Opc == SM83::SHL16rr) {
      // SHL: SLA lo (carry = old bit 7 of lo), RL hi (new bit 0 = carry).
      BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::SLAr), ShiftedLo)
          .addReg(ValLo);
      BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::RLr), ShiftedHi)
          .addReg(ValHi);
    } else if (Opc == SM83::SRL16rr) {
      // SRL: SRL hi (carry = old bit 0 of hi), RR lo (new bit 7 = carry).
      BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::SRLr), ShiftedHi)
          .addReg(ValHi);
      BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::RRr), ShiftedLo)
          .addReg(ValLo);
    } else {
      // SRA: SRA hi (arithmetic; carry = old bit 0 of hi), RR lo.
      BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::SRAr), ShiftedHi)
          .addReg(ValHi);
      BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::RRr), ShiftedLo)
          .addReg(ValLo);
    }

    // Decrement count and branch back.
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::DECr), DecCnt)
        .addReg(CntReg);
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::JRcc))
        .addImm(0).addMBB(LoopMBB); // NZ=0

    // DoneMBB: PHIs for lo/hi result, then REG_SEQUENCE to reconstruct GR16.
    auto InsertPt = DoneMBB->begin();
    Register ResultLo = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register ResultHi = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    BuildMI(*DoneMBB, InsertPt, DL, TII.get(SM83::PHI), ResultLo)
        .addReg(SrcLo).addMBB(MBB).addReg(ShiftedLo).addMBB(LoopMBB);
    BuildMI(*DoneMBB, InsertPt, DL, TII.get(SM83::PHI), ResultHi)
        .addReg(SrcHi).addMBB(MBB).addReg(ShiftedHi).addMBB(LoopMBB);
    BuildMI(*DoneMBB, InsertPt, DL, TII.get(TargetOpcode::REG_SEQUENCE), DstReg)
        .addReg(ResultLo).addImm(SM83::sub_lo)
        .addReg(ResultHi).addImm(SM83::sub_hi);
  } else {
    // 8-bit case: single-register shift loop.

    // Zero-test: LD A, amt; OR A → Z=1 if amt==0.
    BuildMI(*MBB, MI, DL, TII.get(SM83::LDrr), SM83::A).addReg(AmtReg);
    BuildMI(*MBB, MI, DL, TII.get(SM83::ORr)).addReg(SM83::A);
    BuildMI(*MBB, MI, DL, TII.get(SM83::JRcc)).addImm(1).addMBB(DoneMBB);

    Register ValReg     = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register CntReg     = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register ShiftedReg = MRI.createVirtualRegister(&SM83::GPR8RegClass);
    Register DecCnt     = MRI.createVirtualRegister(&SM83::GPR8RegClass);

    // Select the hardware shift opcode.
    unsigned ShiftOpc;
    if (Opc == SM83::SHL8r) ShiftOpc = SM83::SLAr;
    else if (Opc == SM83::SRL8r) ShiftOpc = SM83::SRLr;
    else ShiftOpc = SM83::SRAr;

    // LoopMBB PHIs.
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::PHI), ValReg)
        .addReg(SrcReg).addMBB(MBB).addReg(ShiftedReg).addMBB(LoopMBB);
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::PHI), CntReg)
        .addReg(AmtReg).addMBB(MBB).addReg(DecCnt).addMBB(LoopMBB);

    // Shift by 1, decrement count, branch back.
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(ShiftOpc), ShiftedReg)
        .addReg(ValReg);
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::DECr), DecCnt)
        .addReg(CntReg);
    BuildMI(*LoopMBB, LoopMBB->end(), DL, TII.get(SM83::JRcc))
        .addImm(0).addMBB(LoopMBB); // NZ=0

    // DoneMBB: PHI selects src (amt==0 path) or shifted result.
    BuildMI(*DoneMBB, DoneMBB->begin(), DL, TII.get(SM83::PHI), DstReg)
        .addReg(SrcReg).addMBB(MBB).addReg(ShiftedReg).addMBB(LoopMBB);
  }

  MI.eraseFromParent();
  return DoneMBB;
}

MachineBasicBlock *SM83TargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *MBB) const {
  switch (MI.getOpcode()) {
  case SM83::SHL8r:
  case SM83::SRL8r:
  case SM83::SRA8r:
  case SM83::SHL16rr:
  case SM83::SRL16rr:
  case SM83::SRA16rr:
    return insertShift(MI, MBB);
  default:
    break;
  }

  // Handle SELECT8 / SELECT16 pseudos.
  assert((MI.getOpcode() == SM83::SELECT8 ||
          MI.getOpcode() == SM83::SELECT16) && "Unexpected pseudo");

  const TargetInstrInfo &TII = *Subtarget.getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  // Create diamond control flow for the select.
  MachineFunction *MF = MBB->getParent();
  MachineBasicBlock *FalseMBB = MF->CreateMachineBasicBlock(MBB->getBasicBlock());
  MachineBasicBlock *SinkMBB = MF->CreateMachineBasicBlock(MBB->getBasicBlock());

  MF->insert(++MBB->getIterator(), FalseMBB);
  MF->insert(++FalseMBB->getIterator(), SinkMBB);

  // Transfer successor/PHI info.
  SinkMBB->splice(SinkMBB->begin(), MBB,
                  std::next(MachineBasicBlock::iterator(MI)), MBB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(MBB);

  MBB->addSuccessor(FalseMBB);
  MBB->addSuccessor(SinkMBB);
  FalseMBB->addSuccessor(SinkMBB);

  // Emit the conditional branch.
  unsigned CC = MI.getOperand(3).getImm();
  BuildMI(MBB, DL, TII.get(SM83::JRcc))
      .addImm(CC)
      .addMBB(SinkMBB);

  // FalseMBB just falls through to SinkMBB.

  // Create a PHI in SinkMBB.
  BuildMI(*SinkMBB, SinkMBB->begin(), DL, TII.get(SM83::PHI),
          MI.getOperand(0).getReg())
      .addReg(MI.getOperand(1).getReg())
      .addMBB(MBB)
      .addReg(MI.getOperand(2).getReg())
      .addMBB(FalseMBB);

  MI.eraseFromParent();
  return SinkMBB;
}

const char *SM83TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case SM83ISD::RET:       return "SM83ISD::RET";
  case SM83ISD::CALL:      return "SM83ISD::CALL";
  case SM83ISD::WRAPPER:   return "SM83ISD::WRAPPER";
  case SM83ISD::BRCOND:    return "SM83ISD::BRCOND";
  case SM83ISD::CMP:       return "SM83ISD::CMP";
  case SM83ISD::CMPC:      return "SM83ISD::CMPC";
  case SM83ISD::CMP16:     return "SM83ISD::CMP16";
  case SM83ISD::CMP16EQ:   return "SM83ISD::CMP16EQ";
  case SM83ISD::SELECT_CC: return "SM83ISD::SELECT_CC";
  case SM83ISD::CMPC16:    return "SM83ISD::CMPC16";
  case SM83ISD::RETI:      return "SM83ISD::RETI";
  default:                 return nullptr;
  }
}

//===----------------------------------------------------------------------===//
// Calling Convention Implementation
//===----------------------------------------------------------------------===//

// Assign i8 args to: A, then lo/hi bytes of BC, DE.
// HL is reserved: i16 return value register + indirect-call target.
// Assign i16 args to: BC, DE only.
// HL is never used as an argument register so that indirect calls can always
// safely place the function pointer in HL before CALL __sm83_icall_hl.
// DE is callee-saved, so D/E are not available as argument registers.
static const MCPhysReg ArgGPR8s[] = {SM83::A, SM83::C, SM83::B};
static const MCPhysReg ArgGPR16s[] = {SM83::BC};

static bool CC_SM83(unsigned ValNo, MVT ValVT, MVT LocVT,
                    CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                    Type *OrigTy, CCState &State) {
  if (LocVT == MVT::i8) {
    if (unsigned Reg = State.AllocateReg(ArgGPR8s)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  } else if (LocVT == MVT::i16) {
    if (unsigned Reg = State.AllocateReg(ArgGPR16s)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }
  // Stack.
  unsigned Offset = State.AllocateStack(LocVT == MVT::i16 ? 2 : 1, Align(2));
  State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
  return false;
}

SDValue SM83TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_SM83);

  for (auto &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      MVT VT = VA.getLocVT();
      const TargetRegisterClass *RC =
          VT == MVT::i16 ? &SM83::GR16RegClass : &SM83::GPR8RegClass;
      Register VReg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgVal = DAG.getCopyFromReg(Chain, dl, VReg, VT);
      InVals.push_back(ArgVal);
    } else {
      // Stack argument.
      MachineFrameInfo &MFI = MF.getFrameInfo();
      ISD::ArgFlagsTy Flags = Ins[VA.getValNo()].Flags;
      unsigned Size = Flags.isByVal() ? Flags.getByValSize()
                                      : (VA.getLocVT() == MVT::i16 ? 2 : 1);
      int FI = MFI.CreateFixedObject(Size, VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
      if (Flags.isByVal()) {
        // byval: the callee receives a pointer to its stack copy directly.
        InVals.push_back(FIN);
      } else {
        InVals.push_back(
            DAG.getLoad(VA.getLocVT(), dl, Chain, FIN, MachinePointerInfo()));
      }
    }
  }

  return Chain;
}

bool SM83TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  // SM83 has 3 i16 return regs (HL, BC, DE) = 6 bytes max.
  // Returns larger than 6 bytes use sret (hidden pointer).
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_SM83);
}

SDValue SM83TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
    SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  SmallVector<CCValAssign, 4> RVLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_SM83);

  for (auto &VA : RVLocs) {
    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(),
                             OutVals[VA.getValNo()], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  // Use RETI for interrupt handlers, RET for normal functions.
  unsigned RetOpc = SM83ISD::RET;
  if (MF.getInfo<SM83MachineFunctionInfo>()->isInterruptHandler())
    RetOpc = SM83ISD::RETI;

  return DAG.getNode(RetOpc, dl, MVT::Other, RetOps);
}

// Return the bank number N if `SectionName` is ".romx.bankN" (1..511);
// returns -1 otherwise. Used to detect far-call targets — functions whose
// section attribute places them in a switchable ROM bank.
//
// The cap of 511 covers MBC5's full 9-bit bank register. Banks 1..127 are
// selected by a single write to $2000; banks 128..511 additionally need the
// high bit written to $3000 (see LowerCall's far-call prologue).
static int parseRomxBank(StringRef SectionName) {
  StringRef Prefix = ".romx.bank";
  if (!SectionName.starts_with(Prefix))
    return -1;
  StringRef NumStr = SectionName.drop_front(Prefix.size());
  unsigned N = 0;
  if (NumStr.getAsInteger(10, N))
    return -1;
  if (N < 1 || N > 511)
    return -1;
  return static_cast<int>(N);
}

SDValue
SM83TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                              SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool isVarArg = CLI.IsVarArg;

  MachineFunction &MF = DAG.getMachineFunction();

  // Round 7: detect far calls. If the callee is a direct reference to a
  // function placed in .romx.bankN, we'll emit an inline BANK_SWITCH(N) /
  // CALL / BANK_SWITCH(1) sequence around the call.
  int FarCallBank = -1;
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    if (const Function *F = dyn_cast<Function>(G->getGlobal())) {
      if (F->hasSection())
        FarCallBank = parseRomxBank(F->getSection());
    }
  }

  // Bank-to-bank far calls are not yet supported — the callee must live in
  // bank 0 (unsectioned). Diagnose at compile time rather than produce
  // silently broken code (the banked callee's BANK_SWITCH(1) epilogue
  // would page out its own code mid-execution).
  if (FarCallBank > 0) {
    const Function &Caller = MF.getFunction();
    if (Caller.hasSection() && parseRomxBank(Caller.getSection()) > 0) {
      DAG.getContext()->diagnose(DiagnosticInfoUnsupported(
          Caller,
          "SM83: bank-to-bank far call is not supported (caller in "
          ".romx.bankN). Only callers in ROM bank 0 may call functions in "
          ".romx.bankN."));
    }
  }

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_SM83);

  unsigned NumBytes = CCInfo.getStackSize();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  SmallVector<std::pair<Register, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];

    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
    } else {
      // Stack argument — compute destination pointer at SP + slot offset.
      SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, SM83::SP, MVT::i16);
      SDValue PtrOff = DAG.getNode(ISD::ADD, DL, MVT::i16, StackPtr,
                                   DAG.getConstant(VA.getLocMemOffset(), DL, MVT::i16));
      ISD::ArgFlagsTy Flags = Outs[i].Flags;
      if (Flags.isByVal()) {
        // byval: copy the struct to the stack slot; callee receives the pointer.
        SDValue SizeNode = DAG.getConstant(Flags.getByValSize(), DL, MVT::i16);
        MemOpChains.push_back(
            DAG.getMemcpy(Chain, DL, PtrOff, Arg, SizeNode,
                          Flags.getNonZeroByValAlign(),
                          /*isVolatile=*/false, /*AlwaysInline=*/true,
                          /*CI=*/nullptr, std::nullopt,
                          MachinePointerInfo(), MachinePointerInfo()));
      } else {
        MemOpChains.push_back(
            DAG.getStore(Chain, DL, Arg, PtrOff, MachinePointerInfo()));
      }
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // Round 7 far-call prologue: before setting up argument registers (so
  // this store can't clobber any arg-carrying register), page in the
  // target bank. Emits `ld a, N; ld [$2000], a`. The chain ordering
  // guarantees this executes before the CALL; arg-reg copies follow and
  // get glued to the CALL as usual.
  //
  // Round 8: for banks 128..511 (MBC5), additionally write the upper bit
  // to $3000. Only emitted when needed, to keep low-bank calls at the
  // Round 7 code-size cost.
  if (FarCallBank > 0) {
    SDValue BankLowAddr = DAG.getConstant(0x2000, DL, MVT::i16);
    SDValue BankLowVal = DAG.getConstant(FarCallBank & 0xFF, DL, MVT::i8);
    Chain = DAG.getStore(Chain, DL, BankLowVal, BankLowAddr, MachinePointerInfo());
    if (FarCallBank >= 256) {
      SDValue BankHighAddr = DAG.getConstant(0x3000, DL, MVT::i16);
      SDValue BankHighVal = DAG.getConstant((FarCallBank >> 8) & 0x01, DL, MVT::i8);
      Chain = DAG.getStore(Chain, DL, BankHighVal, BankHighAddr, MachinePointerInfo());
    }
  }

  SDValue InGlue;
  for (auto &RegToPass : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, RegToPass.first, RegToPass.second,
                             InGlue);
    InGlue = Chain.getValue(1);
  }

  // Build the call.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i16);
  } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i16);
  } else {
    // Indirect call through a function pointer. SM83 has no indirect call
    // instruction. Implement via: copy function pointer to HL, then
    // CALL __sm83_icall_hl, which is defined in the SM83 runtime as `jp hl`.
    //
    // HL is reserved from the argument register list precisely for this use.
    // The called function receives return value in HL as usual; the trampoline
    // just transfers control and leaves the callee responsible for setting HL.
    Chain = DAG.getCopyToReg(Chain, DL, SM83::HL, Callee, InGlue);
    InGlue = Chain.getValue(1);
    Callee = DAG.getTargetExternalSymbol("__sm83_icall_hl", MVT::i16);
  }

  Ops.push_back(Callee);

  for (auto &RegToPass : RegsToPass)
    Ops.push_back(DAG.getRegister(RegToPass.first, RegToPass.second.getValueType()));

  // Add a register mask for call-clobbered registers.
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InGlue.getNode())
    Ops.push_back(InGlue);

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(SM83ISD::CALL, DL, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, InGlue, DL);
  InGlue = Chain.getValue(1);

  // Handle return values.
  SmallVector<CCValAssign, 4> RVLocs;
  CCState RetInfo(CallConv, isVarArg, MF, RVLocs, *DAG.getContext());
  RetInfo.AnalyzeCallResult(Ins, RetCC_SM83);

  for (auto &VA : RVLocs) {
    SDValue Val = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(),
                                     VA.getLocVT(), InGlue);
    InVals.push_back(Val.getValue(0));
    Chain  = Val.getValue(1);
    InGlue = Val.getValue(2);
  }

  // Round 7 far-call epilogue: restore bank to 1 (the default). Emits
  // `ld a, 1; ld [$2000], a`. Placed AFTER getCopyFromReg so it reads the
  // return value out of A before A gets clobbered by the immediate load.
  //
  // Round 8: if the prologue wrote the MBC5 upper bit (bank >= 256), also
  // clear $3000 on return so the default bank-1 pager is fully restored.
  if (FarCallBank > 0) {
    SDValue BankLowAddr = DAG.getConstant(0x2000, DL, MVT::i16);
    SDValue BankLowVal = DAG.getConstant(1, DL, MVT::i8);
    Chain = DAG.getStore(Chain, DL, BankLowVal, BankLowAddr, MachinePointerInfo());
    if (FarCallBank >= 256) {
      SDValue BankHighAddr = DAG.getConstant(0x3000, DL, MVT::i16);
      SDValue BankHighVal = DAG.getConstant(0, DL, MVT::i8);
      Chain = DAG.getStore(Chain, DL, BankHighVal, BankHighAddr, MachinePointerInfo());
    }
  }

  return Chain;
}

TargetLowering::ConstraintType
SM83TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    // Specific 8-bit registers.
    case 'a': // A (accumulator)
    case 'b': // B
    case 'c': // C
    case 'd': // D
    case 'e': // E
    case 'h': // H
    case 'l': // L
    // Specific 16-bit pair: HL (only pair that supports indirect LD r,(HL)).
    case 'q':
      return C_Register;
    // Any general register — 8-bit (GPR8) for i8, 16-bit pair (GR16) for i16.
    case 'r':
      return C_RegisterClass;
    // 8-bit unsigned immediate [0, 255].
    case 'I':
      return C_Immediate;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
SM83TargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                  StringRef Constraint,
                                                  MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'a': return {SM83::A, &SM83::GPR8RegClass};
    case 'b': return {SM83::B, &SM83::GR8RegClass};
    case 'c': return {SM83::C, &SM83::GR8RegClass};
    case 'd': return {SM83::D, &SM83::GR8RegClass};
    case 'e': return {SM83::E, &SM83::GR8RegClass};
    case 'h': return {SM83::H, &SM83::GR8RegClass};
    case 'l': return {SM83::L, &SM83::GR8RegClass};
    case 'r':
      if (VT == MVT::i8)
        return {0U, &SM83::GPR8RegClass};
      if (VT == MVT::i16)
        return {0U, &SM83::GR16RegClass};
      break;
    case 'q': // HL specifically — the only pair usable with (HL) indirect ops.
      return {SM83::HL, &SM83::GR16RegClass};
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void SM83TargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  if (Constraint.size() != 1)
    return;
  if (Constraint[0] == 'I') {
    const ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op);
    if (!C)
      return;
    uint64_t Val = C->getZExtValue();
    if (Val > 255)
      return;
    Ops.push_back(DAG.getTargetConstant(Val, SDLoc(Op), Op.getValueType()));
    return;
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

} // end namespace llvm
