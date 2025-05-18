# SIMP Assembler & Simulator

A pair of command-line tools for assembling and cycle-accurately simulating the SIMP (MIPS-like) processor.

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
