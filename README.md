# CHIP-8 TUI(Terminal User Interface) Debugger 

**A C++ Emulator and Real-Time Debugging Tool for the CHIP-8 Architecture.**

*Note: This project is currently in the Architecture & Design phase. Active development is scheduled for Summer 2026.*

**Current Status:**  Planning & Architecture Phase

## The Goal
This project aims to build a **"Transparent Computer."** It renders the emulation inside a Linux terminal using **Ncurses**, visualizing the internal state of the machine alongside the display.

It is designed to be a tool for understanding low-level computing concepts, featuring:
* **Split-Screen Interface:** View the Game, Registers, and Memory Map simultaneously.
* **Instruction Decoding:** Real-time disassembly of opcodes.
* **Cycle-Accurate Stepping:** Manual clock control to debug execution flow.

## Technical Stack
* **Language:** C++ (Focus on memory management & bitwise logic)
* **UI Library:** Ncurses (Terminal User Interface)
* **Platform:** Linux (Mint/Debian)

## Unique Features (Planned)
This project goes beyond standard emulation by integrating advanced debugging tools directly into the terminal:

* ** Time Travel Debugging (Reverse Step):**
    * Utilizes a **Circular Buffer** data structure to store the last 600 CPU states.
    * Allows the user to "rewind" execution frame-by-frame to catch elusive bugs.
* ** Execution Heatmap:**
    * Visualizes "hot" code paths in memory using color-coded overlays.
    * Identifies performance bottlenecks and dead code in real-time.
* ** Live Hex Injector:**
    * Allows direct modification of RAM contents while the emulator is running.
    * Useful for dynamic patching and testing memory exploits.

##  Development Roadmap
This project is being architected in parallel with my **Computer Engineering** coursework (Digital Systems & RISC-V Architecture / Intro to Systems Programming).

* **Phase 1: Research**
    * Mapping CHIP-8 architecture to RISC-V concepts (PC, Registers, Fetch-Decode-Execute cycle).
    * Designing the `CpuCore` class structure.
    * Studying Ncurses window management.
* **Phase 2: Core Implementation**
    * CPU Cycle Logic & Opcode Parsing.
    * Memory & Stack implementation.
    * Basic ROM loading.
* **Phase 3: The "Workbench" (Summer 2026)**
    * Implementing the TUI Dashboard.
    * Adding Debug features (Pause, Step, Rewind).

##  References
* [Cowgod's Chip-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH.HTM)
* *Digital Systems Course* - RISC-V Architecture parallels.
* *Intro to Systems Programming*

