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
#include "llvm/IR/Function.h"
#include "llvm/Support/ErrorHandling.h"

#include "SM83.h"
#include "SM83Subtarget.h"
#include "SM83TargetMachine.h"
#include "MCTargetDesc/SM83MCTargetDesc.h"

namespace llvm {

#include "SM83GenCallingConv.inc"

SM83TargetLowering::SM83TargetLowering(const SM83TargetMachine &TM,
                                       const SM83Subtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // Set up the register classes.
  addRegisterClass(MVT::i8, &SM83::GPR8RegClass);
  addRegisterClass(MVT::i16, &SM83::GR16RegClass);

  // Compute derived properties from the register classes.
  computeRegisterProperties(Subtarget.getRegisterInfo());

  setBooleanContents(ZeroOrOneBooleanContent);
  setSchedulingPreference(Sched::RegPressure);
  setStackPointerRegisterToSaveRestore(SM83::SP);

  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);

  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i8, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);

  // SM83 has no native multiply or divide.
  setOperationAction(ISD::MUL, MVT::i8, Expand);
  setOperationAction(ISD::MUL, MVT::i16, Expand);
  setOperationAction(ISD::SDIV, MVT::i8, Expand);
  setOperationAction(ISD::SDIV, MVT::i16, Expand);
  setOperationAction(ISD::UDIV, MVT::i8, Expand);
  setOperationAction(ISD::UDIV, MVT::i16, Expand);
  setOperationAction(ISD::SREM, MVT::i8, Expand);
  setOperationAction(ISD::SREM, MVT::i16, Expand);
  setOperationAction(ISD::UREM, MVT::i8, Expand);
  setOperationAction(ISD::UREM, MVT::i16, Expand);

  // No sign-extend instruction — expand.
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  // Expand BR_CC and SELECT_CC since we don't have fused compare-branch.
  setOperationAction(ISD::BR_CC, MVT::i8, Expand);
  setOperationAction(ISD::BR_CC, MVT::i16, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i8, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i16, Expand);

  setMinFunctionAlignment(Align(1));
}

SDValue SM83TargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  default:
    report_fatal_error("SM83: unimplemented operand");
  }
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

const char *SM83TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case SM83ISD::RET:
    return "SM83ISD::RET";
  case SM83ISD::CALL:
    return "SM83ISD::CALL";
  case SM83ISD::WRAPPER:
    return "SM83ISD::WRAPPER";
  default:
    return nullptr;
  }
}

SDValue SM83TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  // Stub — no arguments supported yet. Will be implemented in Phase 3.
  if (!Ins.empty())
    report_fatal_error("SM83: function arguments not yet implemented");
  return Chain;
}

SDValue SM83TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
    SelectionDAG &DAG) const {
  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy return values to the physical return registers.
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    Register Reg;
    MVT VT = Outs[i].VT;
    if (VT == MVT::i8)
      Reg = SM83::A;
    else if (VT == MVT::i16)
      Reg = SM83::HL;
    else
      report_fatal_error("SM83: unsupported return type");

    Chain = DAG.getCopyToReg(Chain, dl, Reg, OutVals[i], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(Reg, VT));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  return DAG.getNode(SM83ISD::RET, dl, MVT::Other, RetOps);
}

SDValue
SM83TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                              SmallVectorImpl<SDValue> &InVals) const {
  // Stub — will be implemented in Phase 3.
  report_fatal_error("SM83: function calls not yet implemented");
}

} // end namespace llvm
