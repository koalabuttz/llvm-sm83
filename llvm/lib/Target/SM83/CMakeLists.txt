add_llvm_component_group(SM83)

set(LLVM_TARGET_DEFINITIONS SM83.td)

tablegen(LLVM SM83GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM SM83GenCallingConv.inc -gen-callingconv)
tablegen(LLVM SM83GenDAGISel.inc -gen-dag-isel)
tablegen(LLVM SM83GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM SM83GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM SM83GenSDNodeInfo.inc -gen-sd-node-info)
tablegen(LLVM SM83GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(SM83CommonTableGen)

add_llvm_target(SM83CodeGen
  SM83AsmPrinter.cpp
  SM83ExpandPseudoInsts.cpp
  SM83FrameLowering.cpp
  SM83InstrInfo.cpp
  SM83ISelDAGToDAG.cpp
  SM83ISelLowering.cpp
  SM83MCInstLower.cpp
  SM83RegisterInfo.cpp
  SM83SelectionDAGInfo.cpp
  SM83Subtarget.cpp
  SM83TargetMachine.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  SM83Desc
  SM83Info
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  SM83
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
