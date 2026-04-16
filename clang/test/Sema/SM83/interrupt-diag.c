// RUN: %clang_cc1 -triple sm83-unknown-none -fsyntax-only -verify %s

// Good cases — no diagnostics expected.
__attribute__((interrupt))            void ok1(void) {}
__attribute__((interrupt("vblank")))  void ok2(void) {}
__attribute__((interrupt("lcd")))     void ok3(void) {}
__attribute__((interrupt("timer")))   void ok4(void) {}
__attribute__((interrupt("serial")))  void ok5(void) {}
__attribute__((interrupt("joypad")))  void ok6(void) {}

// Unknown vector name is rejected.
__attribute__((interrupt("nmi"))) void bad1(void) {} // expected-warning{{'interrupt' attribute argument not supported: 'nmi'}}

// Non-void return is rejected.
__attribute__((interrupt("vblank"))) int bad2(void) { return 0; } // expected-warning{{SM83 'interrupt' attribute only applies to functions that have}}

// Parameters are rejected.
__attribute__((interrupt("vblank"))) void bad3(int x) { (void)x; } // expected-warning{{SM83 'interrupt' attribute only applies to functions that have}}

// Too many arguments.
__attribute__((interrupt("vblank", "lcd"))) void bad4(void) {} // expected-error{{'interrupt' attribute takes no more than 1 argument}}
