# Worklet 1 Unit Test Suite (C++)

This directory contains the dedicated unit test suite for **Worklet 1** of the modular x86-32 assembler. Unlike the 100 end-to-end integration tests in the parent directory, these unit tests test the internal C++ data structures and functions directly in memory.

---

## 3 Core Functions Selected for Unit Testing

### 1. `parseMemoryExpressionComponents` (& `validateMemoryExpressionSyntax`)
* **Role**: The core x86-32 addressing mode evaluator.
* **What it tests**:
  - Base register parsing (`[eax]`, `[ebp]`, `[esp]`, etc.).
  - Displacement evaluation (positive, negative, hex, decimal).
  - SIB addressing with scales `1, 2, 4, 8` (`[base + index*scale + disp]`).
  - Commutative order resolution (e.g. `[4*ecx + ebx + 16]`).
  - Memory symbols (`[msg + 4]`, `[eax + data_buffer + 8]`).
  - Rejection of invalid scales (`*0, *3, *5, *6`).
  - Strict syntax verification: rejects subtracting registers (`[ebx - ecx]`), subtracting scaled indices (`[ebx - ecx*4]`), subtracting labels, >2 registers (`[eax + ebx + ecx]`), and >1 label (`[lbl1 + lbl2]`).

### 2. `parseAssemblySource`
* **Role**: The primary front-end assembly parser and intermediate representation (IR) generator.
* **What it tests**:
  - Instruction mapping to typed `w2::Instruction` IR structs and operand categorization (`REG`, `IMM`, `MEM`, `LABEL`, `MEMLABEL`).
  - Dense Label ID assignment: correctly handling label aliases at the same instruction offset and end-of-code labels (`endLabel`).
  - Lowering of pseudo-instructions:
    - `LOAD eax, [ebx]` -> `w2::Op::MOV`
    - `STORE [ebx], eax` -> `w2::Op::MOV`
    - `PUSHA` / `POPA` -> `PUSHAD` / `POPAD` with `0x66` prefix
    - `LODS` / `STOS` -> correct operand count and size
  - Explicit size directives: `BYTE PTR`, `WORD PTR`, `DWORD PTR` attached to memory operands.
  - ESP normalization: swapping ESP from index to base register.
  - Symbol reference tracking (`REF_REL32`, `REF_ABS_IMM`, `REF_ABS_DISP`).
  - Comprehensive error diagnostics: undefined symbols, duplicate labels with line references, reserved words as labels (`eax:`), unsupported mnemonics, rel8 `LOOP` to extern symbols, and missing commas/operands.

### 3. `buildSymbolTable`
* **Role**: The ELF symbol table generator connecting Worklet 2 and Worklet 3.
* **What it tests**:
  - Resolving text label byte offsets from Worklet 2 instruction byte offsets.
  - Validation of declared `global` symbols: catching undeclared/missing definitions with descriptive errors.
  - Implicit `_start` promotion to `STB_GLOBAL`.
  - ELF Rule 2 symbol partitioning:
    - Null symbol (`STN_UNDEF`) at index 0.
    - Section symbols (`STT_SECTION`, `STB_LOCAL`) for `.text`, `.data`, `.bss`.
    - Local labels and constants (`STB_LOCAL`).
    - Global and extern symbols (`STB_GLOBAL`).
  - Exact `sh_info` boundary verification: ensuring `symtab.info == firstNonLocalIndex(model)`.
  - EQU constant attributes (`SHN_ABS`, `STT_NOTYPE`).
  - Extern symbol attributes (`SHN_UNDEF`, value 0, `STB_GLOBAL`).
  - Binary layout: `.symtab` size is a multiple of 16 bytes (`sizeof(Elf32_Sym)`), and `.strtab` starts with a null byte followed by null-terminated strings.

### Bonus Helper: `parseNumericLiteral`
* **Role**: Multi-radix numeric parser supporting decimal (`12345`, `255d`), hex (`0xDEADBEEF`, `0FFH`), binary (`10101010b`), octal (`77o`, `177q`), signs (`-9876`), and rejection of malformed literals (`1234xyz`, `0x123Z`).

---

## Files in this Directory

| File | Description |
|---|---|
| `test_w1_functions.cpp` | Complete C++ unit test suite covering 47 test assertions. |
| `run_unit_tests.py` | Python test runner that compiles and executes the test suite. |
| `README.md` | Detailed documentation of the test suite. |

---

## How to Run

Execute the Python runner:
```bash
python run_unit_tests.py
```

To force a recompile and rerun:
```bash
python run_unit_tests.py --rebuild
```

