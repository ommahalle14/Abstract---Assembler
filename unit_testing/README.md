# Worklet 1: Unit Testing Suite & Theoretical Documentation

This directory contains the dedicated **White Box Unit Testing** infrastructure for **Worklet 1** of the modular x86-32 assembler, organized according to standard Software Engineering principles as outlined in `5_Testing and Maintenance.pdf`.

---

## 1. Testing Concepts & Theory

### What is Unit Testing? (Slides 25–29)
* **Definition**: Unit testing is the **first level of software testing** performed during the coding phase of the Software Development Life Cycle (SDLC) by developers.
* **Objective**: To verify that individual components, methods, or procedures of a program function correctly in isolation before integrating them with other modules.
* **Role in Assembler Development**:
  - Compilers and assemblers contain intricate parsing logic, complex conditional branches, bitwise masks, and memory pointer arithmetic.
  - Testing modules at the system level alone makes it difficult to pinpoint errors. Unit testing isolates each core parsing and table-generating routine, enabling early defect detection (Slide 28: *"Allows developers to detect and fix issues early in the development process"*).

### White Box Testing Techniques Applied (Slides 10–14)
Unit testing in this suite uses **White Box (Clear Box / Glass Box) Testing** methods:
1. **Statement Coverage (Slide 13)**: Every statement in the function is traversed and executed at least once.
2. **Branch Coverage / Condition Testing (Slide 13)**: Both `true` and `false` evaluations of conditional checks (e.g., valid scale factor vs. invalid scale factor; local symbol vs. global symbol) are rigorously exercised.
3. **Path Coverage (Slide 12)**: Control flow paths through decision structures in memory parsing and label resolution are explicitly validated.
4. **Boundary Value Analysis & Error Guessing (Slides 17–18)**:
   - Upper/lower bounds for displacements (-128 to 127 vs 32-bit values).
   - Valid SIB scale factors (`*1, *2, *4, *8`) vs boundary rejections (`*0, *3, *5, *6`).
   - Negative testing: missing operands, duplicate labels, undefined symbols, and forbidden memory syntax.

---

## 2. Organization of the Major Function Test Files

As required, each unit test source file is named directly after the **major function** it tests:

| Major Function Name | Test Source File | Executable Target | Description |
| :--- | :--- | :--- | :--- |
| `parseMemoryExpressionComponents` | [`test_parseMemoryExpressionComponents.cpp`](file:///Users/ommahalle/Sem_5/Software_Eng/finaltesting/Abstract---Assembler/unit_testing/test_parseMemoryExpressionComponents.cpp) | `test_parseMemoryExpressionComponents.exe` | Tests memory operand parsing, SIB scaling, base/index normalization, and numerical literal evaluation. |
| `parseAssemblySource` | [`test_parseAssemblySource.cpp`](file:///Users/ommahalle/Sem_5/Software_Eng/finaltesting/Abstract---Assembler/unit_testing/test_parseAssemblySource.cpp) | `test_parseAssemblySource.exe` | Tests front-end lexing, instruction IR mapping, pseudo-instruction lowerings, label allocation, and error diagnostics. |
| `buildSymbolTable` | [`test_buildSymbolTable.cpp`](file:///Users/ommahalle/Sem_5/Software_Eng/finaltesting/Abstract---Assembler/unit_testing/test_buildSymbolTable.cpp) | `test_buildSymbolTable.exe` | Tests ELF `.symtab` and `.strtab` payload construction, symbol partitioning (local vs. global), and byte address resolution. |

---

## 3. Detailed Breakdown of Major Functions Under Test

### Major Function 1: `parseMemoryExpressionComponents()`
* **Role**: Evaluates the text inside memory brackets `[...]` into structured register, scale, displacement, and symbol variables.
* **Test File**: `test_parseMemoryExpressionComponents.cpp`
* **Test Categories (26 assertions)**:
  1. **Base Register Alone**: Validates `[ebx]`, `[esp]`, `[ebp]`.
  2. **Displacement Evaluation**: Tests positive decimal `[ebx + 4]`, hexadecimal `[edi + 0x1000]`, and negative displacement `[ebp - 8]`.
  3. **SIB Scale Multipliers**: Tests valid hardware scales `*1, *2, *4, *8` (`[ebx + ecx*1]`, `[ebx + edx*2]`, `[esi + edi*4]`, `[eax + ebx*8]`).
  4. **Commutative / Inverted Ordering**: Verifies that inverted syntax like `[4*ecx + ebx + 16]` is correctly parsed into base `EBX` and index `ECX*4`.
  5. **Displacement-Only & Index-Only Forms**: Validates `[edx*4 + 32]`.
  6. **Symbolic Memory Labels**: Validates `[msg_table + 4]` and `[eax + data_buffer + 8]`.
  7. **Negative / Boundary Value Testing**:
     - Rejection of illegal scales: `*0, *3, *5, *6`.
     - Rejection of register subtraction: `[ebx - ecx]`.
     - Rejection of scaled index subtraction: `[ebx - ecx*4]`.
     - Rejection of label subtraction: `[ebx - my_var]`.
     - Rejection of excess registers: `[eax + ebx + ecx]`.
     - Rejection of multiple labels: `[lbl_one + lbl_two]`.
     - Rejection of multiple scaled indices: `[ebx*2 + ecx*4]`.

---

### Major Function 2: `parseAssemblySource()`
* **Role**: Primary front-end tokenization, instruction lowerings, symbol reference tracking, and IR generation.
* **Test File**: `test_parseAssemblySource.cpp`
* **Test Categories (12 assertions)**:
  1. **Instruction & Operand Enum Mapping**: Verifies translation from source text into `w2::Instruction` and strongly typed `w2::Operand` (`REG`, `IMM`, `MEM`, `LABEL`).
  2. **Label Extraction & Dense Label ID Assignment**:
     - Resolves forward and backward branch targets.
     - Confirms label aliasing (multiple labels at the same instruction offset share the same dense ID).
     - Confirms `endLabel` allocation for labels defined after the final instruction.
  3. **Lowering of Pseudo-Instructions**:
     - `LOAD` $\rightarrow$ `MOV`
     - `STORE` $\rightarrow$ `MOV`
     - `PUSHA` / `POPA` $\rightarrow$ `PUSHAD` / `POPAD` with `0x66` prefix
     - `LODS` / `STOS` $\rightarrow$ explicit size assignment (32-bit default).
  4. **Explicit Width Directives**: Verifies that `BYTE PTR`, `WORD PTR`, and `DWORD PTR` set `Operand::size` to 8, 16, or 32 bits respectively.
  5. **ESP Normalization**: Verifies that `[ecx + esp]` is automatically swapped to base `ESP` and index `ECX` (since x86 hardware disallows `ESP` as a SIB index).
  6. **Symbol Reference Tracking**: Classifies symbol references into `REF_REL32` (relative branch), `REF_ABS_IMM` (immediate value), and `REF_ABS_DISP` (memory displacement).
  7. **Error Diagnostics (Negative Path Coverage)**:
     - Undefined symbols.
     - Duplicate symbol definitions.
     - Reserved register names used as labels (`eax:`).
     - Unsupported mnemonics.
     - `LOOP` instructions referencing external symbols (rel8 boundary violation).
     - Comma syntax errors (`mov eax ebx`, `mov eax,`).

---

### Major Function 3: `buildSymbolTable()`
* **Role**: Takes instruction byte offsets from Worklet 2, resolves label values, and constructs the ELF32 `.symtab` binary payload and `.strtab` string table.
* **Test File**: `test_buildSymbolTable.cpp`
* **Test Categories (6 assertions)**:
  1. **Exact Offset Resolution**: Resolves text label addresses using byte offsets calculated by Worklet 2's encoder.
  2. **Undeclared Global Symbol Validation**: Catches declared `global` symbols that have no corresponding definition, reporting a clean error.
  3. **Implicit `_start` Promotion**: Promotes `_start` to `STB_GLOBAL` automatically if not explicitly marked.
  4. **ELF Symbol Partitioning & `sh_info` Boundary**:
     - Enforces the ELF specification rule that **all Local symbols (`STB_LOCAL`) must precede Non-Local/Global symbols (`STB_GLOBAL`)**.
     - Verifies that `symtab.info` is set to the exact index of the first non-local symbol.
  5. **EQU Constants & Extern Attributes**:
     - `equ` constants assigned to absolute section `SHN_ABS` with type `STT_NOTYPE`.
     - `extern` symbols assigned to undefined section `SHN_UNDEF` with binding `STB_GLOBAL`.
  6. **Binary Layout & Sizing**:
     - `.symtab` size is an exact multiple of 16 bytes (`sizeof(Elf32_Sym)`).
     - `.strtab` begins with a null byte (`0x00`) followed by null-terminated ASCII strings.

---

## 4. How to Run the Unit Tests

### Automated Suite Execution (Python Runner)
To compile and run all three unit test suites in sequence:
```bash
python3 run_unit_tests.py
```
*(Or `python run_unit_tests.py` on Windows)*

To force a clean recompile:
```bash
python3 run_unit_tests.py --rebuild
```

### Manual Individual Execution
You can also compile and run any individual major function test directly:

#### 1. Test `parseMemoryExpressionComponents`:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I.. test_parseMemoryExpressionComponents.cpp ../worklet2.cpp -o test_parseMemoryExpressionComponents.exe
./test_parseMemoryExpressionComponents.exe
```

#### 2. Test `parseAssemblySource`:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I.. test_parseAssemblySource.cpp ../worklet2.cpp -o test_parseAssemblySource.exe
./test_parseAssemblySource.exe
```

#### 3. Test `buildSymbolTable`:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I.. test_buildSymbolTable.cpp ../worklet2.cpp -o test_buildSymbolTable.exe
./test_buildSymbolTable.exe
```

