# Integration Test Suite (`tests/`)

This directory contains the **100 end-to-end assembly test cases** (`test_001.asm` to `test_100.asm`) designed to validate the modular x86-32 assembler pipeline.

---

## Test Categorization

### 1. Valid Test Cases (001 - 060)
*Expected behavior*: The entire pipeline assembles and links the source code into valid ELF32 relocatable objects (`.o`) and flat binaries (`.bin`) with exit code `0`.

| Range | Feature / Instructions Tested |
|---|---|
| `test_001 - test_005` | Data movement (`MOV` between 8/16/32-bit registers, immediates, memory, SIB). |
| `test_006 - test_015` | Arithmetic & logic (`ADD`, `SUB`, `ADC`, `SBB`, `CMP`, `AND`, `OR`, `XOR`, `TEST`). |
| `test_016 - test_025` | Unary operations, shifts & rotates (`INC`, `DEC`, `NEG`, `NOT`, `SHL`, `SHR`, `SAR`, `ROL`, `ROR`). |
| `test_026 - test_035` | Multiplications & divisions (`MUL`, `DIV`, `IMUL`, `IDIV`, `CDQ`, `CWDE`, `CBW`). |
| `test_036 - test_045` | Control flow & jumps (unconditional `JMP`, `CALL`, `RET`, all 16 `Jcc` conditional branches, `LOOP`/`LOOPE`/`LOOPNE`). |
| `test_046 - test_055` | String operations (`MOVS`, `CMPS`, `SCAS`, `LODS`, `STOS` with B/W/D widths), bit manipulation (`BT`, `BTS`, `BTR`, `BTC`, `BSF`, `BSR`). |
| `test_056 - test_060` | Directives & complex sections (`.data` `db/dw/dd`, `.bss` `resb`, `equ` constants, external symbols, `global`). |

---

### 2. Error Test Cases (061 - 100)
*Expected behavior*: The assembler catches the invalid condition, halts the pipeline, and reports a descriptive error with a non-zero exit code (`!= 0`).

| Range | Subsystem | Error Types Caught |
|---|---|---|
| `test_061 - test_075` | **Worklet 3 (Front-Half)** | Unknown sections, duplicate `equ`, label-equ conflicts, invalid directives, negative `times`, count overflow, missing `resb` operands, out-of-range `db`/`dw`, division by zero. |
| `test_076 - test_089` | **Worklet 1 (Front-Half)** | Unclosed brackets, invalid numbers, reserved words as labels (`eax:`), unsupported mnemonics, undefined symbols, 16-bit registers as memory addresses, rel8 `LOOP` to extern, labels as operands to arithmetic. |
| `test_090` | **Worklet 2 (Encoder)** | ESP used as an index register in SIB. |
| `test_091 - test_099` | **Worklet 1 (Front-Half)** | Subtracted registers (`[ebx - ecx]`), subtracted scaled index, >2 registers, >1 label, subtracted label, missing comma, missing operand after comma, >3 operands, duplicate symbol. |
| `test_100` | **Worklet 1 (Back-Half)** | Declared `global` symbol that is never defined in the source. |

---

## Regenerating the Tests

The `.asm` test files are generated programmatically by:

```bash
python generate_tests.py
```

## Running the Integration Tests

To run all 100 tests in a single automated pass:

```bash
python run_tests.py
```

The runner automatically builds `assembler.exe` (if needed), executes Worklet 1 (`--w1`), Worklet 2 (`--w2`), and the full pipeline (`-o out.o --bin out.bin --listing --dump-elf`) for each test, verifies expected pass/fail status, and generates reports in `responses/`.
