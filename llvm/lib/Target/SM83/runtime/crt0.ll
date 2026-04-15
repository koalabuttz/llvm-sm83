; crt0.ll — SM83 startup routine in LLVM IR
;
; Compiled to sm83-crt0.o by build-llvm-sm83.sh using:
;   llc -march=sm83 -filetype=obj crt0.ll -o sm83-crt0.o
;
; The asm parser doesn't exist yet, so crt0.s cannot be assembled directly.
; This IR is the functional equivalent.
;
; Startup sequence:
;   1. Zero the BSS region (_bss_start.._bss_end)
;   2. Copy initialised .data from ROM (_data_load) to WRAM (_data_start.._data_end)
;   3. Call main()
;   4. Halt loop (main() should never return on a Game Boy)
;
; Stack pointer and DI are not touched here; the DMG/GBC boot ROM sets
; SP=$FFFE and disables interrupts before jumping to $0100.

@_bss_start  = external global i8
@_bss_end    = external global i8
@_data_start = external global i8
@_data_end   = external global i8
@_data_load  = external global i8

declare void @main()

define void @_start() section ".text._start" {
entry:
  ; ---- Zero BSS -------------------------------------------------------
  ; Skip if BSS is empty (common in ROM-only programs).
  %bss_start_i = ptrtoint ptr @_bss_start to i16
  %bss_end_i   = ptrtoint ptr @_bss_end   to i16
  %bss_len     = sub i16 %bss_end_i, %bss_start_i
  %bss_nz      = icmp ne i16 %bss_len, 0
  br i1 %bss_nz, label %bss_loop, label %bss_done

bss_loop:
  %bpp      = phi ptr [ @_bss_start, %entry ], [ %bpp_next, %bss_loop ]
  store i8 0, ptr %bpp
  %bpp_next = getelementptr i8, ptr %bpp, i16 1
  %at_end   = icmp eq ptr %bpp_next, @_bss_end
  br i1 %at_end, label %bss_done, label %bss_loop

bss_done:
  ; ---- Copy .data from ROM → WRAM -------------------------------------
  %data_start_i = ptrtoint ptr @_data_start to i16
  %data_end_i   = ptrtoint ptr @_data_end   to i16
  %data_len     = sub i16 %data_end_i, %data_start_i
  %data_nz      = icmp ne i16 %data_len, 0
  br i1 %data_nz, label %data_loop, label %data_done

data_loop:
  %src      = phi ptr [ @_data_load,  %bss_done  ], [ %src_next, %data_loop ]
  %dst      = phi ptr [ @_data_start, %bss_done  ], [ %dst_next, %data_loop ]
  %byte     = load i8, ptr %src
  store i8 %byte, ptr %dst
  %src_next = getelementptr i8, ptr %src, i16 1
  %dst_next = getelementptr i8, ptr %dst, i16 1
  %done     = icmp eq ptr %dst_next, @_data_end
  br i1 %done, label %data_done, label %data_loop

data_done:
  ; ---- Call user entry point ------------------------------------------
  call void @main()
  br label %halt

halt:
  ; main() returned — halt forever (shouldn't happen on a well-written GB game).
  br label %halt
}
