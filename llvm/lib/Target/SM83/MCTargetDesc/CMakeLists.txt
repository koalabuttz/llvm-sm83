add_llvm_component_library(LLVMSM83Desc
  SM83InstPrinter.cpp
  SM83MCAsmInfo.cpp
  SM83MCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  SM83Info
  Support
  TargetParser

  ADD_TO_COMPONENT
  SM83
)

add_dependencies(LLVMSM83Desc SM83CommonTableGen)
