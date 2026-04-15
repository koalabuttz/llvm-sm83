add_llvm_component_library(LLVMSM83Disassembler
  SM83Disassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  SM83Info
  Support

  ADD_TO_COMPONENT
  SM83
)
