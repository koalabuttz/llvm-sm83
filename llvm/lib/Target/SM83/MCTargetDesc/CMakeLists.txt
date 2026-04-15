add_llvm_component_library(LLVMSM83Desc
  SM83AsmBackend.cpp
  SM83ELFObjectWriter.cpp
  SM83InstPrinter.cpp
  SM83MCAsmInfo.cpp
  SM83MCCodeEmitter.cpp
  SM83MCTargetDesc.cpp

  LINK_COMPONENTS
  BinaryFormat
  MC
  SM83Info
  Support
  TargetParser

  ADD_TO_COMPONENT
  SM83
)

add_dependencies(LLVMSM83Desc SM83CommonTableGen)
