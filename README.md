# Modular x86-32 Assembler

A robust, modular x86-32 assembler architecture built in modern C++ (C++17) producing flat binary (`.bin`) and relocatable ELF32 object files (`.o`).

---

## Architecture Overview

The assembler is structured into three distinct, decoupled worklets operating across 5 pipeline phases:

```
[ .asm Source ]
       │
       ▼
┌────────────────────────────────────────────────────────┐
│ Phase 1: Source Splitting (Worklet 3 Front-Half)       │
│ - Parses sections (.text, .data, .bss)                 │
│ - Directives: global, extern, equ, db/dw/dd, resb/times│
└──────────────────────────┬─────────────────────────────┘
                           │ .text lines
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 2: Lexing, Parsing & IR (Worklet 1 Front-Half)   │
│ - Tokenizes and parses instructions into w2::Instruction│
│ - Resolves labels, assigns dense IDs, tracks SymRefs   │
└──────────────────────────┬─────────────────────────────┘
                           │ w2::Program IR
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 3: Machine Code Encoding (Worklet 2)             │
│ - Generates ModR/M, SIB, prefixes, opcodes, and disp   │
│ - Produces machine code bytes and exact instruction offs│
└──────────────────────────┬─────────────────────────────┘
                           │ Byte offsets & Machine code
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 4: Symbol Table Generation (Worklet 1 Back-Half) │
│ - Resolves label addresses from Worklet 2 offsets      │
│ - Generates ELF-compliant .symtab and .strtab payloads │
└──────────────────────────┬─────────────────────────────┘
                           │ ObjectModel (.symtab + .strtab)
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 5: ELF Relocation & Object Generation (Worklet 3)│
│ - Emits ELF32 Header (Elf32_Ehdr) & Section Headers    │
│ - Builds relocations (R_386_32, R_386_PC32)            │
│ - Validates ELF structures (checkElf) and writes .o    │
└────────────────────────────────────────────────────────┘
```

---

## Building the Assembler

Compile the assembler using any C++17 compatible compiler:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -o assembler.exe main.cpp worklet2.cpp
```

---

## Command Line Usage

### Full Pipeline (Default)
```bash
# Generate relocatable ELF32 object file (.o):
.\assembler.exe program.asm -o program.o

# Generate flat binary:
.\assembler.exe program.asm -o program.bin

# Generate ELF object, flat binary, and disassembly listing simultaneously:
.\assembler.exe program.asm -o program.o --bin program.bin --listing
```

### Worklet-Specific Execution
```bash
# Run Worklet 1 only (tokenize, parse, display IR & labels):
.\assembler.exe program.asm --w1

# Run Worklet 2 only (encode to machine code, display listing):
.\assembler.exe program.asm --w2 -o program.bin

# Run Worklet 3 / Pipeline with ELF header dump:
.\assembler.exe program.asm --dump-elf
```

### Inspecting ELF Output
```bash
readelf -h -S out.o
readelf -s out.o
objdump -d out.o
```

---

## Directory Structure

```
assembler_new/
├── main.cpp                  # Pipeline driver and CLI entry point
├── worklet1.cpp              # Worklet 1: Parser, Front-End IR, and Symbol Table generator
├── worklet2.h                # Worklet 2: Strongly-typed instruction enums & encoder API
├── worklet2.cpp              # Worklet 2: x86-32 machine code encoder implementation
├── worklet3.cpp              # Worklet 3: Directives, section splitter, and ELF32 generator
├── INSTRUCTION_SET.md        # Comprehensive x86-32 ISA reference supported by assembler
├── tests/                    # 100 End-to-end integration test cases (test_001 to test_100)
├── responses/                # Captured worklet execution outputs for all 100 tests
├── unit_tests_worklet1/      # Dedicated C++ unit test suite for Worklet 1 core functions
├── generate_tests.py         # Test generator script for the 100 integration tests
├── run_tests.py              # Automated test runner for the 100 integration tests
├── run_tests.bat             # Batch runner for the 100 integration tests
└── run_tests.ps1             # PowerShell runner for the 100 integration tests
```

---

## Running the Tests

### 1. Worklet 1 Dedicated Unit Tests (In-Memory C++ Tests)
Runs 47 isolated unit tests covering `parseMemoryExpressionComponents`, `parseAssemblySource`, and `buildSymbolTable`:
```bash
python unit_tests_worklet1/run_unit_tests.py
```

### 2. Full Pipeline 100 Integration Tests
Executes all 60 valid and 40 error test cases through all 3 worklets:
```bash
python run_tests.py
```
*(or run `.\run_tests.bat` / `.\run_tests.ps1`)*