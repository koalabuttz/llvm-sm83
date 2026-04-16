#!/usr/bin/env python3
"""Minimal SM83 CPU simulator for headless testing.

Supports the subset of SM83 instructions that the LLVM backend emits.
Not a full Game Boy emulator — no LCD timing, DMA, or audio.
Hardware registers ($FF00-$FFFF) are faked: LY ($FF44) returns 144
to satisfy VBlank polling loops.

Usage:
    from sm83sim import SM83Sim
    sim = SM83Sim(open('test.gb', 'rb').read())
    sim.run(max_steps=500000)
    assert sim.mem[0xC100] == 100
"""

import struct
import sys


class SM83Sim:
    def __init__(self, rom_data):
        self.mem = bytearray(0x10000)
        # Load ROM into $0000-$7FFF
        rom_len = min(len(rom_data), 0x8000)
        self.mem[0:rom_len] = rom_data[:rom_len]

        # Registers: 8-bit A, B, C, D, E, H, L, F; 16-bit SP, PC
        self.a = self.b = self.c = self.d = self.e = self.h = self.l = 0
        self.f = 0  # flags: Z=bit7, N=bit6, H=bit5, C=bit4
        self.sp = 0xDFFF
        self.pc = 0x0150  # _start entry point (after ROM header)
        self.halted = False
        self.ime = False  # interrupt master enable
        self.steps = 0

    # --- Flag helpers ---
    @property
    def flag_z(self):
        return bool(self.f & 0x80)

    @flag_z.setter
    def flag_z(self, v):
        self.f = (self.f & ~0x80) | (0x80 if v else 0)

    @property
    def flag_n(self):
        return bool(self.f & 0x40)

    @flag_n.setter
    def flag_n(self, v):
        self.f = (self.f & ~0x40) | (0x40 if v else 0)

    @property
    def flag_h(self):
        return bool(self.f & 0x20)

    @flag_h.setter
    def flag_h(self, v):
        self.f = (self.f & ~0x20) | (0x20 if v else 0)

    @property
    def flag_c(self):
        return bool(self.f & 0x10)

    @flag_c.setter
    def flag_c(self, v):
        self.f = (self.f & ~0x10) | (0x10 if v else 0)

    # --- 16-bit register pairs ---
    def get_bc(self):
        return (self.b << 8) | self.c

    def set_bc(self, v):
        v &= 0xFFFF
        self.b, self.c = v >> 8, v & 0xFF

    def get_de(self):
        return (self.d << 8) | self.e

    def set_de(self, v):
        v &= 0xFFFF
        self.d, self.e = v >> 8, v & 0xFF

    def get_hl(self):
        return (self.h << 8) | self.l

    def set_hl(self, v):
        v &= 0xFFFF
        self.h, self.l = v >> 8, v & 0xFF

    def get_af(self):
        return (self.a << 8) | (self.f & 0xF0)

    def set_af(self, v):
        v &= 0xFFFF
        self.a, self.f = v >> 8, v & 0xF0

    # --- Memory access ---
    def read8(self, addr):
        addr &= 0xFFFF
        # Fake hardware registers
        if addr == 0xFF44:  # LY — current scanline
            return 144  # always in VBlank
        return self.mem[addr]

    # --- Interrupt dispatch ---
    #
    # Minimal model: IF ($FF0F) and IE ($FFFF) live in self.mem like any
    # other RAM (writes go through write8, reads through read8). VBlank is
    # periodically raised by run() via vblank_period. Other interrupt
    # sources (LCD/Timer/Serial/Joypad) can be raised by tests writing to
    # $FF0F directly.
    def _check_interrupts(self):
        """Handle pending interrupts. Returns True if state changed."""
        pending = self.mem[0xFF0F] & self.mem[0xFFFF] & 0x1F
        if not pending:
            return False
        if self.ime:
            # Dispatch to the lowest-numbered pending vector.
            # Bit 0=VBlank ($40), 1=LCD ($48), 2=Timer ($50),
            # 3=Serial ($58), 4=Joypad ($60).
            bit = (pending & -pending).bit_length() - 1
            self.mem[0xFF0F] = self.mem[0xFF0F] & ~(1 << bit) & 0xFF
            self.ime = False
            self.halted = False
            self.push16(self.pc)
            self.pc = 0x40 + bit * 8
            return True
        if self.halted:
            # IME off but pending interrupt still wakes HALT.
            self.halted = False
            return True
        return False

    def write8(self, addr, val):
        addr &= 0xFFFF
        val &= 0xFF
        # ROM is read-only ($0000-$7FFF)
        if addr < 0x8000:
            return
        self.mem[addr] = val

    def read16(self, addr):
        return self.read8(addr) | (self.read8(addr + 1) << 8)

    def write16(self, addr, val):
        self.write8(addr, val & 0xFF)
        self.write8(addr + 1, (val >> 8) & 0xFF)

    def fetch8(self):
        v = self.read8(self.pc)
        self.pc = (self.pc + 1) & 0xFFFF
        return v

    def fetch16(self):
        lo = self.fetch8()
        hi = self.fetch8()
        return (hi << 8) | lo

    # --- ALU helpers ---
    def _add_a(self, val, carry=0):
        r = self.a + val + carry
        self.flag_z = (r & 0xFF) == 0
        self.flag_n = False
        self.flag_h = ((self.a & 0xF) + (val & 0xF) + carry) > 0xF
        self.flag_c = r > 0xFF
        self.a = r & 0xFF

    def _sub_a(self, val, carry=0):
        r = self.a - val - carry
        self.flag_z = (r & 0xFF) == 0
        self.flag_n = True
        self.flag_h = ((self.a & 0xF) - (val & 0xF) - carry) < 0
        self.flag_c = r < 0
        self.a = r & 0xFF

    def _get_r8(self, idx):
        """Get 8-bit register by table index: B=0 C=1 D=2 E=3 H=4 L=5 [HL]=6 A=7"""
        return [self.b, self.c, self.d, self.e, self.h, self.l,
                self.read8(self.get_hl()), self.a][idx]

    def _set_r8(self, idx, val):
        val &= 0xFF
        if idx == 0: self.b = val
        elif idx == 1: self.c = val
        elif idx == 2: self.d = val
        elif idx == 3: self.e = val
        elif idx == 4: self.h = val
        elif idx == 5: self.l = val
        elif idx == 6: self.write8(self.get_hl(), val)
        elif idx == 7: self.a = val

    def _get_rr(self, idx):
        """Get 16-bit register pair: BC=0 DE=1 HL=2 SP=3"""
        return [self.get_bc, self.get_de, self.get_hl, lambda: self.sp][idx]()

    def _set_rr(self, idx, val):
        [self.set_bc, self.set_de, self.set_hl,
         lambda v: setattr(self, 'sp', v & 0xFFFF)][idx](val)

    def _get_rr_push(self, idx):
        """Push/pop pair: BC=0 DE=1 HL=2 AF=3"""
        return [self.get_bc, self.get_de, self.get_hl, self.get_af][idx]()

    def _set_rr_push(self, idx, val):
        [self.set_bc, self.set_de, self.set_hl, self.set_af][idx](val)

    def _check_cc(self, cc):
        """Check condition code: NZ=0 Z=1 NC=2 C=3"""
        if cc == 0: return not self.flag_z
        if cc == 1: return self.flag_z
        if cc == 2: return not self.flag_c
        if cc == 3: return self.flag_c
        return False

    def push16(self, val):
        self.sp = (self.sp - 2) & 0xFFFF
        self.write16(self.sp, val)

    def pop16(self):
        val = self.read16(self.sp)
        self.sp = (self.sp + 2) & 0xFFFF
        return val

    # --- Execution ---
    def step(self):
        """Execute one instruction. Returns False if halted."""
        if self.halted:
            return False

        op = self.fetch8()

        # NOP
        if op == 0x00:
            pass

        # LD rr, nn
        elif op in (0x01, 0x11, 0x21, 0x31):
            rr = (op >> 4) & 3
            self._set_rr(rr, self.fetch16())

        # LD [BC], A
        elif op == 0x02:
            self.write8(self.get_bc(), self.a)
        # LD [DE], A
        elif op == 0x12:
            self.write8(self.get_de(), self.a)

        # INC rr
        elif op in (0x03, 0x13, 0x23, 0x33):
            rr = (op >> 4) & 3
            self._set_rr(rr, (self._get_rr(rr) + 1) & 0xFFFF)

        # DEC rr
        elif op in (0x0B, 0x1B, 0x2B, 0x3B):
            rr = (op >> 4) & 3
            self._set_rr(rr, (self._get_rr(rr) - 1) & 0xFFFF)

        # INC r
        elif op & 0xC7 == 0x04:
            r = (op >> 3) & 7
            v = (self._get_r8(r) + 1) & 0xFF
            self.flag_z = v == 0
            self.flag_n = False
            self.flag_h = (v & 0xF) == 0
            self._set_r8(r, v)

        # DEC r
        elif op & 0xC7 == 0x05:
            r = (op >> 3) & 7
            v = (self._get_r8(r) - 1) & 0xFF
            self.flag_z = v == 0
            self.flag_n = True
            self.flag_h = (v & 0xF) == 0xF
            self._set_r8(r, v)

        # LD r, n
        elif op & 0xC7 == 0x06:
            r = (op >> 3) & 7
            self._set_r8(r, self.fetch8())

        # RLCA
        elif op == 0x07:
            c = (self.a >> 7) & 1
            self.a = ((self.a << 1) | c) & 0xFF
            self.flag_z = False
            self.flag_n = False
            self.flag_h = False
            self.flag_c = bool(c)

        # RRCA
        elif op == 0x0F:
            c = self.a & 1
            self.a = (c << 7) | (self.a >> 1)
            self.flag_z = False
            self.flag_n = False
            self.flag_h = False
            self.flag_c = bool(c)

        # RLA
        elif op == 0x17:
            c = int(self.flag_c)
            self.flag_c = bool(self.a & 0x80)
            self.a = ((self.a << 1) | c) & 0xFF
            self.flag_z = False
            self.flag_n = False
            self.flag_h = False

        # RRA
        elif op == 0x1F:
            c = int(self.flag_c)
            self.flag_c = bool(self.a & 1)
            self.a = (c << 7) | (self.a >> 1)
            self.flag_z = False
            self.flag_n = False
            self.flag_h = False

        # LD A, [BC]
        elif op == 0x0A:
            self.a = self.read8(self.get_bc())
        # LD A, [DE]
        elif op == 0x1A:
            self.a = self.read8(self.get_de())

        # ADD HL, rr
        elif op in (0x09, 0x19, 0x29, 0x39):
            rr = (op >> 4) & 3
            hl = self.get_hl()
            val = self._get_rr(rr)
            r = hl + val
            self.flag_n = False
            self.flag_h = ((hl & 0xFFF) + (val & 0xFFF)) > 0xFFF
            self.flag_c = r > 0xFFFF
            self.set_hl(r & 0xFFFF)

        # LD [HL+], A / LD [HL-], A / LD A, [HL+] / LD A, [HL-]
        elif op == 0x22:
            self.write8(self.get_hl(), self.a)
            self.set_hl(self.get_hl() + 1)
        elif op == 0x32:
            self.write8(self.get_hl(), self.a)
            self.set_hl(self.get_hl() - 1)
        elif op == 0x2A:
            self.a = self.read8(self.get_hl())
            self.set_hl(self.get_hl() + 1)
        elif op == 0x3A:
            self.a = self.read8(self.get_hl())
            self.set_hl(self.get_hl() - 1)

        # DAA
        elif op == 0x27:
            a = self.a
            if not self.flag_n:
                if self.flag_h or (a & 0xF) > 9:
                    a += 6
                if self.flag_c or a > 0x9F:
                    a += 0x60
                    self.flag_c = True
            else:
                if self.flag_h:
                    a = (a - 6) & 0xFF
                if self.flag_c:
                    a -= 0x60
            self.a = a & 0xFF
            self.flag_z = self.a == 0
            self.flag_h = False

        # CPL
        elif op == 0x2F:
            self.a = (~self.a) & 0xFF
            self.flag_n = True
            self.flag_h = True

        # SCF
        elif op == 0x37:
            self.flag_n = False
            self.flag_h = False
            self.flag_c = True

        # CCF
        elif op == 0x3F:
            self.flag_n = False
            self.flag_h = False
            self.flag_c = not self.flag_c

        # JR n / JR cc, n
        elif op == 0x18:
            offset = self.fetch8()
            if offset > 127: offset -= 256
            if offset == -2:  # JR to self = infinite loop
                self.halted = True
                return False
            self.pc = (self.pc + offset) & 0xFFFF
        elif op in (0x20, 0x28, 0x30, 0x38):
            cc = (op >> 3) & 3
            offset = self.fetch8()
            if offset > 127: offset -= 256
            if self._check_cc(cc):
                self.pc = (self.pc + offset) & 0xFFFF

        # HALT
        elif op == 0x76:
            self.halted = True
            return False

        # LD r, r (0x40-0x7F except 0x76=HALT)
        elif 0x40 <= op <= 0x7F:
            dst = (op >> 3) & 7
            src = op & 7
            self._set_r8(dst, self._get_r8(src))

        # ALU A, r (0x80-0xBF)
        elif 0x80 <= op <= 0xBF:
            alu = (op >> 3) & 7
            val = self._get_r8(op & 7)
            self._alu_op(alu, val)

        # ALU A, n (0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE)
        elif op & 0xC7 == 0xC6:
            alu = (op >> 3) & 7
            val = self.fetch8()
            self._alu_op(alu, val)

        # RET cc
        elif op in (0xC0, 0xC8, 0xD0, 0xD8):
            cc = (op >> 3) & 3
            if self._check_cc(cc):
                self.pc = self.pop16()

        # RET
        elif op == 0xC9:
            self.pc = self.pop16()

        # RETI
        elif op == 0xD9:
            self.pc = self.pop16()
            self.ime = True

        # JP nn
        elif op == 0xC3:
            addr = self.fetch16()
            # JP to self = infinite loop, treat as halt
            if addr == self.pc - 3:
                self.halted = True
                return False
            self.pc = addr

        # JP cc, nn
        elif op in (0xC2, 0xCA, 0xD2, 0xDA):
            cc = (op >> 3) & 3
            addr = self.fetch16()
            if self._check_cc(cc):
                self.pc = addr

        # CALL nn
        elif op == 0xCD:
            addr = self.fetch16()
            self.push16(self.pc)
            self.pc = addr

        # CALL cc, nn
        elif op in (0xC4, 0xCC, 0xD4, 0xDC):
            cc = (op >> 3) & 3
            addr = self.fetch16()
            if self._check_cc(cc):
                self.push16(self.pc)
                self.pc = addr

        # PUSH rr
        elif op in (0xC5, 0xD5, 0xE5, 0xF5):
            rr = (op >> 4) & 3
            self.push16(self._get_rr_push(rr))

        # POP rr
        elif op in (0xC1, 0xD1, 0xE1, 0xF1):
            rr = (op >> 4) & 3
            self._set_rr_push(rr, self.pop16())

        # RST
        elif op & 0xC7 == 0xC7:
            addr = op & 0x38
            self.push16(self.pc)
            self.pc = addr

        # LDH [n], A
        elif op == 0xE0:
            n = self.fetch8()
            self.write8(0xFF00 + n, self.a)

        # LDH A, [n]
        elif op == 0xF0:
            n = self.fetch8()
            self.a = self.read8(0xFF00 + n)

        # LD [C], A
        elif op == 0xE2:
            self.write8(0xFF00 + self.c, self.a)

        # LD A, [C]
        elif op == 0xF2:
            self.a = self.read8(0xFF00 + self.c)

        # LD [nn], A
        elif op == 0xEA:
            self.write8(self.fetch16(), self.a)

        # LD A, [nn]
        elif op == 0xFA:
            self.a = self.read8(self.fetch16())

        # ADD SP, n (signed)
        elif op == 0xE8:
            n = self.fetch8()
            if n > 127: n -= 256
            r = self.sp + n
            self.flag_z = False
            self.flag_n = False
            self.flag_h = ((self.sp & 0xF) + (n & 0xF)) & 0x10 != 0
            self.flag_c = ((self.sp & 0xFF) + (n & 0xFF)) & 0x100 != 0
            self.sp = r & 0xFFFF

        # LD HL, SP+n
        elif op == 0xF8:
            n = self.fetch8()
            if n > 127: n -= 256
            r = self.sp + n
            self.flag_z = False
            self.flag_n = False
            self.flag_h = ((self.sp & 0xF) + (n & 0xF)) & 0x10 != 0
            self.flag_c = ((self.sp & 0xFF) + (n & 0xFF)) & 0x100 != 0
            self.set_hl(r & 0xFFFF)

        # LD SP, HL
        elif op == 0xF9:
            self.sp = self.get_hl()

        # LD [nn], SP
        elif op == 0x08:
            addr = self.fetch16()
            self.write16(addr, self.sp)

        # JP HL
        elif op == 0xE9:
            self.pc = self.get_hl()

        # DI / EI
        elif op == 0xF3:
            self.ime = False
        elif op == 0xFB:
            self.ime = True

        # CB prefix
        elif op == 0xCB:
            self._exec_cb()

        else:
            raise RuntimeError(
                f"Unimplemented opcode ${op:02X} at PC=${self.pc - 1:04X} "
                f"(step {self.steps})")

        self.steps += 1
        return True

    def _alu_op(self, alu, val):
        """Execute ALU operation: ADD=0 ADC=1 SUB=2 SBC=3 AND=4 XOR=5 OR=6 CP=7"""
        if alu == 0:    # ADD
            self._add_a(val)
        elif alu == 1:  # ADC
            self._add_a(val, int(self.flag_c))
        elif alu == 2:  # SUB
            self._sub_a(val)
        elif alu == 3:  # SBC
            self._sub_a(val, int(self.flag_c))
        elif alu == 4:  # AND
            self.a &= val
            self.flag_z = self.a == 0
            self.flag_n = False
            self.flag_h = True
            self.flag_c = False
        elif alu == 5:  # XOR
            self.a ^= val
            self.flag_z = self.a == 0
            self.flag_n = False
            self.flag_h = False
            self.flag_c = False
        elif alu == 6:  # OR
            self.a |= val
            self.flag_z = self.a == 0
            self.flag_n = False
            self.flag_h = False
            self.flag_c = False
        elif alu == 7:  # CP
            r = self.a - val
            self.flag_z = (r & 0xFF) == 0
            self.flag_n = True
            self.flag_h = ((self.a & 0xF) - (val & 0xF)) < 0
            self.flag_c = r < 0

    def _exec_cb(self):
        """Execute CB-prefixed instruction."""
        op = self.fetch8()
        r = op & 7
        val = self._get_r8(r)
        group = (op >> 6) & 3
        bit = (op >> 3) & 7

        if group == 0:  # Rotate/Shift
            if bit == 0:    # RLC
                c = (val >> 7) & 1
                val = ((val << 1) | c) & 0xFF
                self.flag_c = bool(c)
            elif bit == 1:  # RRC
                c = val & 1
                val = (c << 7) | (val >> 1)
                self.flag_c = bool(c)
            elif bit == 2:  # RL
                c = int(self.flag_c)
                self.flag_c = bool(val & 0x80)
                val = ((val << 1) | c) & 0xFF
            elif bit == 3:  # RR
                c = int(self.flag_c)
                self.flag_c = bool(val & 1)
                val = (c << 7) | (val >> 1)
            elif bit == 4:  # SLA
                self.flag_c = bool(val & 0x80)
                val = (val << 1) & 0xFF
            elif bit == 5:  # SRA
                self.flag_c = bool(val & 1)
                val = (val & 0x80) | (val >> 1)
            elif bit == 6:  # SWAP
                val = ((val & 0xF) << 4) | ((val >> 4) & 0xF)
                self.flag_c = False
            elif bit == 7:  # SRL
                self.flag_c = bool(val & 1)
                val = val >> 1
            self.flag_z = val == 0
            self.flag_n = False
            self.flag_h = False
            self._set_r8(r, val)

        elif group == 1:  # BIT
            self.flag_z = not bool(val & (1 << bit))
            self.flag_n = False
            self.flag_h = True

        elif group == 2:  # RES
            self._set_r8(r, val & ~(1 << bit))

        elif group == 3:  # SET
            self._set_r8(r, val | (1 << bit))

    def run(self, max_steps=500000, vblank_period=5000):
        """Run until deterministic halt or step limit.

        vblank_period: simulated CPU steps between VBlank triggers. Set to
        0 or None to disable VBlank simulation (matches pre-interrupt
        behaviour for tests that don't care).

        Termination:
          - HALT with IME=0 or IE=0: program is done, returns True.
          - Pending interrupt with IME=1: dispatches to the vector.
          - Pending interrupt with HALT+IME=0: unhalts and continues.
          - Step limit exceeded: returns False.
        """
        vblank_clock = 0
        while self.steps < max_steps:
            if vblank_period:
                vblank_clock += 1
                if vblank_clock >= vblank_period:
                    self.mem[0xFF0F] |= 0x01
                    vblank_clock = 0

            self._check_interrupts()

            if self.halted:
                # No pending interrupt, IME off, or IE=0 — permanent halt.
                if not self.ime or (self.mem[0xFFFF] & 0x1F) == 0:
                    return True
                # Otherwise burn a cycle waiting for the next VBlank raise.
                # Must bump `steps` so max_steps still terminates us if
                # vblank_period is 0 or the ISR never re-enables IME.
                self.steps += 1
                continue

            if not self.step():
                # step() reported halt/infinite-loop. Re-evaluate whether
                # further interrupts could wake us.
                if not self.ime or (self.mem[0xFFFF] & 0x1F) == 0:
                    return True

        return False  # hit step limit
