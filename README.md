# SIMP Assembler & Simulator

An assembler and cycle-accurate simulator for the SIMP (MIPS-like) processor, including I/O and interrupts.

---

- **assembler.c**        — C implementation of the SIMP assembler
- **simulator.c**        — C implementation of the SIMP simulator
- **Makefile**           — Rules to build both `asm` and `sim` executables
- **README.md**          

---

## Assembler

**Purpose:**  
Translate a SIMP assembly source file into two plain-text memory images:

- **Instruction memory image**
- **Data memory image**

**Key features:**
- Support for decimal, hexadecimal (`0x…`) and label immediates
- `.word` pseudo-instruction for embedding data words
- Strips `#`-style comments
- Simple command-line interface

## Assembler Inputs
`./asm program.asm imemin.txt dmemin.txt`

- `program.asm`  
  The SIMP assembly source file. Contains instructions, labels, and `.word` directives.  
- `imin.txt`  
  Path to output the instruction memory image (plain-text, one 12-hex-digit word per line).  
- `dmemin.txt`  
  Path to output the data memory image (plain-text, one 8-hex-digit word per line).  

---

## Simulator

**Purpose:**  
Run a cycle-by-cycle simulation of the assembled program, including I/O and interrupts.

**Key features:**  
- **Fetch-Decode-Execute loop** with correct cycle counts
- **Data memory accesses** (word-aligned loads/stores)
- **Disk I/O** via disk image file
- **Timer interrupts**, **disk interrupts**, and **external interrupts**
- **Memory-mapped I/O** for:
  - LEDs
  - 7-segment displays
  - Framebuffer monitor output (YUV format)
- **Execution tracing**: register dumps, hardware-register traces, total cycle count
- Final dumps of registers, memory, disk, and display outputs

## Simulator Inputs
`./sim imemin.txt dmemin.txt diskin.txt irq2in.txt dmemout.txt regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt diskout.txt monitor.txt monitor.yuv`

- `imin.txt`
  Instruction memory image produced by the assembler (plain-text, one 12-hex-digit word per line).
- `dmemin.txt`
  Initial data memory image produced by the assembler (plain-text, one 8-hex-digit word per line).
- `diskin.txt`
  Initial disk contents: 128 sectors × 512 bytes, represented as 8-hex-digit words (16 words per sector), one per line.
- `irq2in.txt`
  External IRQ2 schedule: one decimal cycle number per line indicating when IRQ2 fires.
- `dmemout.txt`
  Path to write the final data memory image (same format as `dmemin.txt`).
- `regout.txt`
  Path to write registers R3–R15 (one 8-hex-digit value per line).
- `trace.txt`
  Per-instruction execution log: PC (3 hex), instruction (12 hex), registers R0–R15 (8 hex each).
- `hwregtrace.txt`
  Hardware-register I/O trace (cycle number & register writes).
- `cycles.txt`
  Total cycle count (single decimal number).
- `leds.txt`
  LED output changes: `<cycle> <8-hex>` per line.
- `display7seg.txt`
  7-segment display changes: `<cycle> <8-hex>` per line.
- `diskout.txt`
  Final disk image (same format as `diskin.txt`).
- `monitor.txt`
  Final frame buffer dump: 256×256 pixels, one 2-hex-digit YUV byte per line.
- `monitor.yuv`
  Binary YUV file for graphical display.
