; crt0.ll — Minimal SM83 startup in LLVM IR
; Sets stack pointer, calls main(), halts.

target datalayout = "e-p:16:8-i8:8-i16:8-n8-S8"
target triple = "sm83-unknown-none"

declare void @main()

; Write to stack pointer via inline constant store
; SP is set by the linker script to $DFFF; we trust the hardware reset state
; or rely on the Game Boy boot ROM to set SP = $FFFE.
; _start just calls main and halts.
define void @_start() naked noreturn nounwind {
entry:
  call void @main()
  br label %halt

halt:
  br label %halt  ; infinite loop (HALT would be better but needs asm support)
}
