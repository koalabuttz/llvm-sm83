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
  addRegisterClass(MVT::i8, &SM83::GPR8RegClass);
  addRegisterClass(MVT::i16, &SM83::GR16RegClass);

  computeRegisterProperties(Subtarget.getRegisterInfo());

  setBooleanContents(ZeroOrOneBooleanContent);
  setSchedulingPreference(Sched::RegPressure);
  setStackPointerRegisterToSaveRestore(SM83::SP);


  // Custom lowering.
  setOperationAction(ISD::GlobalAddress, MVT::i16, Custom);
  setOperationAction(ISD::ExternalSymbol, MVT::i16, Custom);
  setOperationAction(ISD::BR_CC, MVT::i8, Custom);
  setOperationAction(ISD::BR_CC, MVT::i16, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i8, Custom);
  setOperationAction(ISD::SELECT_CC, MVT::i16, Custom);
  setOperationAction(ISD::SETCC, MVT::i8, Custom);
  setOperationAction(ISD::SETCC, MVT::i16, Custom);
  setOperationAction(ISD::SELECT, MVT::i8, Expand);
  setOperationAction(ISD::SELECT, MVT::i16, Expand);

  // Expand operations SM83 can't do natively.
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i8, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);

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
  setOperationAction(ISD::MULHS, MVT::i8, Expand);
  setOperationAction(ISD::MULHU, MVT::i8, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i8, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i8, Expand);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::ROTL, MVT::i8, Expand);
  setOperationAction(ISD::ROTR, MVT::i8, Expand);
  setOperationAction(ISD::CTPOP, MVT::i8, Expand);
  setOperationAction(ISD::CTLZ, MVT::i8, Expand);
  setOperationAction(ISD::CTTZ, MVT::i8, Expand);
  setOperationAction(ISD::BSWAP, MVT::i16, Expand);

  setOperationAction(ISD::SHL_PARTS, MVT::i8, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i8, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i8, Expand);

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

SDValue SM83TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);

  bool NeedSignFlip;
  unsigned SM83CC = mapCondCode(CC, LHS, RHS, DAG, DL, NeedSignFlip);

  SDValue Cmp;
  if (LHS.getValueType() == MVT::i16) {
    if (CC == ISD::SETEQ || CC == ISD::SETNE) {
      // 16-bit equality: XOR both bytes inside the pseudo, set Z flag.
      Cmp = DAG.getNode(SM83ISD::CMP16EQ, DL, MVT::Glue, LHS, RHS);
    } else {
      // 16-bit ordering: CP lo bytes + SBC hi bytes → C flag = (lhs < rhs).
      // For signed: flip bit 15 of both (XOR 0x8000) to convert to unsigned ordering.
      if (NeedSignFlip) {
        SDValue Flip = DAG.getConstant(0x8000, DL, MVT::i16);
        LHS = DAG.getNode(ISD::XOR, DL, MVT::i16, LHS, Flip);
        RHS = DAG.getNode(ISD::XOR, DL, MVT::i16, RHS, Flip);
      }
      Cmp = DAG.getNode(SM83ISD::CMP16, DL, MVT::Glue, LHS, RHS);
    }
  } else {
    if (NeedSignFlip)
      applySignFlip(LHS, RHS, DAG, DL);
    Cmp = DAG.getNode(SM83ISD::CMP, DL, MVT::Glue, LHS, RHS);
  }

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

  SDValue Cmp;
  if (LHS.getValueType() == MVT::i16) {
    if (CC == ISD::SETEQ || CC == ISD::SETNE) {
      Cmp = DAG.getNode(SM83ISD::CMP16EQ, DL, MVT::Glue, LHS, RHS);
    } else {
      if (NeedSignFlip) {
        SDValue Flip = DAG.getConstant(0x8000, DL, MVT::i16);
        LHS = DAG.getNode(ISD::XOR, DL, MVT::i16, LHS, Flip);
        RHS = DAG.getNode(ISD::XOR, DL, MVT::i16, RHS, Flip);
      }
      Cmp = DAG.getNode(SM83ISD::CMP16, DL, MVT::Glue, LHS, RHS);
    }
  } else {
    if (NeedSignFlip)
      applySignFlip(LHS, RHS, DAG, DL);
    Cmp = DAG.getNode(SM83ISD::CMP, DL, MVT::Glue, LHS, RHS);
  }

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

  SDValue Cmp;
  if (LHS.getValueType() == MVT::i16) {
    if (CC == ISD::SETEQ || CC == ISD::SETNE) {
      Cmp = DAG.getNode(SM83ISD::CMP16EQ, DL, MVT::Glue, LHS, RHS);
    } else {
      if (NeedSignFlip) {
        SDValue Flip = DAG.getConstant(0x8000, DL, MVT::i16);
        LHS = DAG.getNode(ISD::XOR, DL, MVT::i16, LHS, Flip);
        RHS = DAG.getNode(ISD::XOR, DL, MVT::i16, RHS, Flip);
      }
      Cmp = DAG.getNode(SM83ISD::CMP16, DL, MVT::Glue, LHS, RHS);
    }
  } else {
    if (NeedSignFlip)
      applySignFlip(LHS, RHS, DAG, DL);
    Cmp = DAG.getNode(SM83ISD::CMP, DL, MVT::Glue, LHS, RHS);
  }

  SDValue CCVal = DAG.getConstant(SM83CC, DL, MVT::i8);
  SDValue Zero = DAG.getConstant(0, DL, MVT::i8);
  SDValue One = DAG.getConstant(1, DL, MVT::i8);
  return DAG.getNode(SM83ISD::SELECT_CC, DL, MVT::i8, One, Zero, CCVal, Cmp);
}

MachineBasicBlock *SM83TargetLowering::EmitInstrWithCustomInserter(
    MachineInstr &MI, MachineBasicBlock *MBB) const {
  // Handle SELECT8 pseudo.
  assert(MI.getOpcode() == SM83::SELECT8 && "Unexpected pseudo");

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
  default:                 return nullptr;
  }
}

//===----------------------------------------------------------------------===//
// Calling Convention Implementation
//===----------------------------------------------------------------------===//

// Assign i8 args to: A, then lo/hi bytes of BC, DE, HL.
// Assign i16 args to: BC, DE, HL.
static const MCPhysReg ArgGPR8s[] = {SM83::A, SM83::C, SM83::B,
                                     SM83::E, SM83::D, SM83::L, SM83::H};
static const MCPhysReg ArgGPR16s[] = {SM83::BC, SM83::DE, SM83::HL};

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
  unsigned Offset = State.AllocateStack(LocVT == MVT::i16 ? 2 : 1, Align(1));
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
      // Stack argument — load from stack slot.
      MachineFrameInfo &MFI = MF.getFrameInfo();
      unsigned Size = VA.getLocVT() == MVT::i16 ? 2 : 1;
      int FI = MFI.CreateFixedObject(Size, VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
      InVals.push_back(
          DAG.getLoad(VA.getLocVT(), dl, Chain, FIN, MachinePointerInfo()));
    }
  }

  return Chain;
}

SDValue SM83TargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
    SelectionDAG &DAG) const {
  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    MVT VT = Outs[i].VT;
    Register Reg;
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
      // Stack argument.
      SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, SM83::SP, MVT::i16);
      SDValue PtrOff = DAG.getNode(ISD::ADD, DL, MVT::i16, StackPtr,
                                   DAG.getConstant(VA.getLocMemOffset(), DL, MVT::i16));
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, Arg, PtrOff, MachinePointerInfo()));
    }
  }

  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue InGlue;
  for (auto &RegToPass : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, RegToPass.first, RegToPass.second,
                             InGlue);
    InGlue = Chain.getValue(1);
  }

  // Build the call.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);

  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i16);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i16);

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
  for (unsigned i = 0, e = Ins.size(); i != e; ++i) {
    Register Reg;
    MVT VT = Ins[i].VT;
    if (VT == MVT::i8)
      Reg = SM83::A;
    else if (VT == MVT::i16)
      Reg = SM83::HL;
    else
      report_fatal_error("SM83: unsupported return type in call");

    SDValue Val = DAG.getCopyFromReg(Chain, DL, Reg, VT, InGlue);
    InVals.push_back(Val.getValue(0));
    Chain = Val.getValue(1);
    InGlue = Val.getValue(2);
  }

  return Chain;
}

} // end namespace llvm
