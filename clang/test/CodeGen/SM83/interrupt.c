// RUN: %clang_cc1 -triple sm83-unknown-none -emit-llvm %s -o - | FileCheck %s

// Bare interrupt attribute: emits the "interrupt" fn attr, no section.
__attribute__((interrupt)) void generic_isr(void) { }
// CHECK: define{{.*}} void @generic_isr(){{.*}}#[[ATTR:[0-9]+]]
// CHECK-NOT: @generic_isr(){{.*}}section

// interrupt("vblank") — routes to .isr.vblank section.
__attribute__((interrupt("vblank"))) void vb(void) { }
// CHECK: define{{.*}} void @vb(){{.*}}section ".isr.vblank"

__attribute__((interrupt("lcd"))) void lcd(void) { }
// CHECK: define{{.*}} void @lcd(){{.*}}section ".isr.lcd"

__attribute__((interrupt("timer"))) void tm(void) { }
// CHECK: define{{.*}} void @tm(){{.*}}section ".isr.timer"

__attribute__((interrupt("serial"))) void sr(void) { }
// CHECK: define{{.*}} void @sr(){{.*}}section ".isr.serial"

__attribute__((interrupt("joypad"))) void jp(void) { }
// CHECK: define{{.*}} void @jp(){{.*}}section ".isr.joypad"

// All six share the same fn-attr group because "interrupt" is the only
// SM83-level distinguishing attribute — the section is per-function metadata,
// not a fn-attr.
// CHECK: attributes #[[ATTR]] = {{{.*"interrupt".*}}}
