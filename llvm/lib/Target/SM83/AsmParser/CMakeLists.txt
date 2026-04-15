add_llvm_component_library(LLVMSM83AsmParser
  SM83AsmParser.cpp

  LINK_COMPONENTS
  SM83Desc
  SM83Info
  CodeGenTypes
  MC
  MCParser
  Support

  ADD_TO_COMPONENT
  SM83
  )
