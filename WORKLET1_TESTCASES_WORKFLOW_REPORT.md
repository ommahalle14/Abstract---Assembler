# Worklet 1 Execution Workflow & Test Case Analysis Report

**Document Scope:** Technical Workflow Walkthrough for Valid Tests (001–020) and Worklet 1 Error Tests (061–070)  
**Target Module:** Worklet 1 (Lexer, Parser, Front-End IR & Symbol Table Generator)  
**Associated PDF Report:** [`Worklet1_Testcases_Workflow_Execution_Analysis.pdf`](file:///d:/Downloads/assembler_new/Worklet1_Testcases_Workflow_Execution_Analysis.pdf)  
**Assembler Version:** x86-32 Modular Assembler (C++17)

---

## 1. Worklet 1 Execution Pipeline Workflow

When executing any assembly program, Worklet 1 operates across two non-adjacent phases:

```
[ Raw Assembly File ]
         │
         ▼
[ Phase 1: Source Splitting (Worklet 3) ]
         │ .text lines
         ▼
┌────────────────────────────────────────────────────────┐
│ Phase 2: Worklet 1 Front-End Processing                │
│                                                        │
│ 1. Tokenize Line (tokenizeSourceLine)                  │
│    - Strips whitespace & comments                      │
│    - Emits Token stream with (type, text, line, col)   │
│                                                        │
│ 2. Label Discovery & Dense ID Assignment               │
│    - Extracts LABEL tokens (name:)                     │
│    - Assigns contiguous dense IDs (idByInstr)          │
│    - Detects endLabel if labels point to EOF           │
│                                                        │
│ 3. Instruction Mapping & Lowering                      │
│    - Identifies Opcode (MOV, ADD, PUSH, etc.)          │
│    - Lowers pseudo-ops: LOAD/STORE -> MOV              │
│    - Lowers PUSHA/POPA -> PUSHAD/POPAD (prefix 0x66)   │
│    - Normalizes ESP from index to base register        │
│                                                        │
│ 4. Operand Categorization                              │
│    - REG: General registers (32/16/8-bit)              │
│    - IMM: Multi-radix immediate numbers                │
│    - MEM: ModR/M & SIB addressing                      │
│    - LABEL / MEMLABEL: Symbolic references             │
│                                                        │
│ 5. Symbol Reference Recording                          │
│    - REF_REL32 (relative jumps/calls)                  │
│    - REF_ABS_IMM (immediate label address)            │
│    - REF_ABS_DISP (memory label displacement)          │
└──────────────────────────┬─────────────────────────────┘
                           │ w2::Program (instructions + labels)
                           ▼
[ Phase 3: Machine Code Encoder (Worklet 2) ]
                           │ Instruction Byte Offsets
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 4: Worklet 1 Back-Half Processing                │
│                                                        │
│ 6. ELF Symbol Table Generation (buildSymbolTable)      │
│    - Binds label addresses to Worklet 2 byte offsets   │
│    - Validates declared 'global' symbols               │
│    - Promotes _start to STB_GLOBAL                     │
│    - Partitions symbols: STN_UNDEF -> SECTION -> LOCAL │
│      -> GLOBAL -> EXTERN                               │
│    - Calculates symtab.info boundary                   │
│    - Emits binary .symtab (16-byte Elf32_Sym) & .strtab│
└────────────────────────────────────────────────────────┘
```

---

## 2. Valid Test Cases 001 to 020: Worklet 1 Workflow Walkthrough

### Test 001: MOV Between Registers (32-bit, 16-bit, and 8-bit)
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      mov eax, ebx
      mov ecx, edx
      mov esi, edi
      mov ebp, esp
      mov ax, bx
      mov cx, dx
      mov si, di
      mov bp, sp
      mov al, bl
      mov cl, dl
      mov ah, bh
      mov ch, dh
      mov al, ah
      mov bl, ch
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Phase 1 Ingestion:** Receives 16 instruction lines from Worklet 3.
  2. **Lexing:** Identifies `_start:` as a label, followed by 14 `mov` statements and 1 `ret` statement.
  3. **Label Discovery:** Discovers label `_start` at instruction index 0. Assigns dense ID `0`.
  4. **Instruction Parsing:** Queries `lookupRegister()` for each operand. Identifies 32-bit registers (EAX..ESP, `size = 32`), 16-bit registers (AX..SP, `size = 16`), and 8-bit registers (AL..BL, `size = 8`). Emits 15 `w2::Instruction` IR structs with `op = w2::Op::MOV`, `operandCount = 2`.
  5. **IR Output:** Cleanly hands 15 instructions to Worklet 2. Zero errors.

---

### Test 002: MOV Register with All Immediate Formats
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      mov eax, 100
      mov ebx, 0x1000
      mov ecx, 0FFh
      mov edx, 10101010b
      mov esi, 77o
      mov edi, 177q
      mov ebp, 255d
      mov eax, -500
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Lexing:** Tokenizes numbers into `TokenType::IMMEDIATE`.
  2. **Numeric Evaluation:** Invokes `parseNumericLiteral()` across all radices: decimal (`100`, `255d`, `-500`), hex (`0x1000`, `0FFh`), binary (`10101010b`), octal (`77o`, `177q`).
  3. **IR Mapping:** Tags operands as `kind = OperandKind::IMM` and stores evaluated 32-bit integer values in `imm`.
  4. **Output:** 9 IR instructions generated. Zero errors.

---

### Test 003: Memory Addressing with [base] and [base + disp]
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      mov eax, [ebx]
      mov eax, [ebp]
      mov eax, [esp]
      mov eax, [ebx + 4]
      mov eax, [ecx - 8]
      mov eax, [edx + 0x1000]
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Lexing:** Brackets `[...]` are grouped into `TokenType::MEMORY`.
  2. **Memory Parsing:** `parseMemoryExpressionComponents()` evaluates bracket contents. In `[ebx]`, extracts base register `EBX`, disp=0. In `[ebx + 4]`, extracts disp=+4. In `[ecx - 8]`, extracts disp=-8. In `[edx + 0x1000]`, extracts hex disp=+4096.
  3. **IR Output:** Instantiates `w2::OperandKind::MEM` with `mem.base = Reg::EBX`, `mem.index = Reg::NONE`, `mem.disp = disp`. Zero errors.

---

### Test 004: SIB Memory Addressing [base + index*scale + disp]
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      mov eax, [ebx + ecx*1]
      mov eax, [ebx + edx*2]
      mov eax, [esi + edi*4]
      mov eax, [eax + ebx*8]
      mov eax, [ebx + ecx*4 + 16]
      mov eax, [ebp + edx*2 - 12]
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Memory Evaluation:** Recognizes complex SIB terms containing `*`.
  2. **Scale Validation:** Asserts multiplier is in `{1, 2, 4, 8}`.
  3. **Commutative Decomposition:** Deconstructs `base + index*scale + disp`. Assigns base and index register enums.
  4. **IR Output:** Emits `MemOperand` structs with scale multipliers intact. Zero errors.

---

### Test 005: Index*scale + disp and Absolute 32-bit Displacement
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      mov eax, [ecx*4 + 16]
      mov eax, [edx*2 - 8]
      mov eax, [0]
      mov ebx, [0x1000]
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Base-less SIB:** In `[ecx*4 + 16]`, detects missing base register. Sets `base = Reg::NONE`, `index = Reg::ECX`, `scale = 4`, `disp = 16`.
  2. **Pure Displacement:** In `[0]` and `[0x1000]`, detects absence of both base and index registers. Sets both to `Reg::NONE` and assigns absolute displacement. Zero errors.

---

### Test 006: Sized Memory Operands (BYTE PTR, WORD PTR, DWORD PTR)
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      mov BYTE PTR [ebx], 10
      mov WORD PTR [ebx], 20
      mov DWORD PTR [ebx], 30
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Size Keywords:** Lexer reads `BYTE`, `WORD`, `DWORD` followed by `PTR`.
  2. **Operand Tagging:** Sets explicit bit size (`8`, `16`, `32`) on the destination memory operand.
  3. **IR Output:** Passes explicit size tags to Worklet 2, ensuring proper opcode and ModR/M selection. Zero errors.

---

### Test 007: XCHG Across All Operand Combinations
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      xchg eax, ebx
      xchg ecx, edx
      xchg eax, [ebx]
      xchg [ebx], ecx
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Mnemonic Lookup:** Resolves `XCHG` to `w2::Op::XCHG`.
  2. **Operand Parsing:** Validates 2 operands (REG-REG, REG-MEM, MEM-REG).
  3. **Optimization Flagging:** Recognizes `EAX` as operand 0 or 1 for short 1-byte opcode emission in Worklet 2. Zero errors.

---

### Test 008: PUSH and POP with 32-bit and 16-bit Registers
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      push eax
      push ebx
      push ax
      push bx
      pop bx
      pop ax
      pop ebx
      pop eax
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Arity Validation:** Confirms `PUSH` and `POP` take exactly 1 operand.
  2. **Size Differentiation:** 32-bit registers get `size = 32`; 16-bit registers get `size = 16` (triggering prefix `0x66` in encoder). Zero errors.

---

### Test 009: PUSH and POP with Memory Operands
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      push dword ptr [eax]
      push word ptr [ebx + 4]
      pop dword ptr [eax]
      pop word ptr [ebx + 4]
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Memory Evaluation:** Parses memory operands with explicit size directives (`DWORD PTR`, `WORD PTR`).
  2. **IR Output:** Emits single-operand memory instructions (`kind = OperandKind::MEM`). Zero errors.

---

### Test 010: Stack State Manipulation (PUSHAD, POPAD, PUSHA, POPA, PUSHFD, POPFD)
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      pushad
      popad
      pusha
      popa
      pushfd
      popfd
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Pseudo-Op Lowering:** Detects 16-bit pseudo-ops `PUSHA` and `POPA`. Lowers them to `PUSHAD` and `POPAD` with prefix `0x66`.
  2. **No-Operand Mapping:** Sets `operandCount = 0` for all stack state instructions. Zero errors.

---

### Test 011: LEA Instruction with Simple, SIB, and Label Addressing
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      lea eax, [ebx + 4]
      lea ecx, [edx + esi*4 + 8]
      lea edx, target_label
  target_label:
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Mnemonic Mapping:** Maps `LEA` to `w2::Op::LEA`.
  2. **Label Addressing:** For `lea edx, target_label`, identifies bare label as memory reference (`symIsMemory = true`), creates `MEMLABEL` operand, and records a `REF_ABS_DISP` relocation reference. Zero errors.

---

### Test 012: LOAD and STORE Pseudo-Instructions
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      LOAD eax, [ebx]
      LOAD ecx, 100
      STORE [ebx], edx
      STORE [ebp - 4], eax
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Lowering:** Detects `LOAD dst, src` and lowers to `MOV dst, src`. Detects `STORE dst, src` and lowers to `MOV dst, src`.
  2. **IR Emission:** Emits standard `w2::Op::MOV` instructions with 2 operands. Zero errors.

---

### Test 013: Accumulator Conversions and Flag Transfers
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      cbw
      cwde
      cwd
      cdq
      lahf
      sahf
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Extended Lookup:** Resolves no-operand mnemonics via `lookupExtendedMnemonic()`.
  2. **IR Output:** Emits instructions with `operandCount = 0`. Zero errors.

---

### Test 014: ADD Arithmetic Across 32, 16, and 8-bit Operands
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      add eax, ebx
      add ecx, 100
      add byte ptr [eax], 5
      add [ebx], edx
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Sets `op = w2::Op::ADD`.
  2. **Type Propagation:** Verifies operand pairings (r-r, r-i, m-i, m-r). Preserves bit sizes for immediate sign-extension optimization in Worklet 2. Zero errors.

---

### Test 015: ADC (Add with Carry) Across All Operand Shapes
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      adc eax, ebx
      adc ecx, 1
      adc edx, [ebx + 4]
      adc dword ptr [eax], 10
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Resolves `ADC` to `w2::Op::ADC`.
  2. **IR Output:** Emits 2-operand ALU instructions covering register, memory, and immediate combinations. Zero errors.

---

### Test 016: SUB Arithmetic Across Registers, Memory, and Immediates
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      sub eax, ebx
      sub ecx, 500
      sub dword ptr [ebx], 100
      sub edx, [esi]
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Resolves `SUB` to `w2::Op::SUB`.
  2. **Operand Extraction:** Emits typed IR instructions with ModR/M and immediate attributes. Zero errors.

---

### Test 017: SBB (Subtract with Borrow) Across All Operand Forms
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      sbb eax, ebx
      sbb edx, 1
      sbb eax, [ebx + 8]
      sbb dword ptr [edi], 5
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Resolves `SBB` to `w2::Op::SBB`.
  2. **Multi-Word Arithmetic:** Prepares operands for multi-precision borrow subtraction. Zero errors.

---

### Test 018: CMP Comparisons Across 32, 16, and 8-bit Registers and Memory
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      cmp eax, ebx
      cmp ax, bx
      cmp al, bl
      cmp ecx, 100
      cmp dword ptr [ebx], 0
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Resolves `CMP` to `w2::Op::CMP`.
  2. **Flags Evaluation:** Preserves size attributes (8, 16, 32) ensuring Worklet 2 emits non-destructive comparison opcodes. Zero errors.

---

### Test 019: AND Bitwise Logic Across All Sizes and Operand Types
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      and eax, 0xFF
      and ebx, ecx
      and ax, 0x0F0F
      and byte ptr [edx], 0x80
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Resolves `AND` to `w2::Op::AND`.
  2. **Mask Evaluation:** Immediate bitmasks are evaluated and packed into `IMM` operands. Zero errors.

---

### Test 020: OR Bitwise Logic Across All Sizes
* **Source Code:**
  ```assembly
  section .text
  global _start
  _start:
      or eax, 0x10
      or ebx, ecx
      or ax, bx
      or dword ptr [esi], 0x80000000
      ret
  ```
* **Worklet 1 Execution Workflow:**
  1. **Opcode Mapping:** Resolves `OR` to `w2::Op::OR`.
  2. **IR Output:** Emits 2-operand logical instructions supporting 32-bit masks and register pairings. Zero errors.

---

## 3. Worklet 1 Error Tests 061 to 070: Detailed Failure Workflow

### Test 061: Missing Closing Bracket `]` in Memory Operand
* **Source Code:**
  ```assembly
  section .text
  _start:
      mov eax, [ebx + 4
  ```
* **Worklet 1 Failure Workflow:**
  1. **Lexical Scanning:** `tokenizeSourceLine()` encounters `[` at column 14.
  2. **Delimiter Search:** It loops forward scanning for `]`. It hits the newline without encountering `]`.
  3. **Error Triggered:** Emits `Lexical Error at line 3, column 14: missing closing ']' for memory operand`.
  4. **Pipeline Halting:** The error is pushed to `out.errors`. `out.ok` is set to `false`. Worklet 1 stops execution and exits with status 1. Worklet 2 is never invoked.

---

### Test 062: Invalid Immediate Literal
* **Source Code:**
  ```assembly
  section .text
  _start:
      mov eax, 12345xyz
  ```
* **Worklet 1 Failure Workflow:**
  1. **Lexical Scanning:** Tokenizer identifies `12345xyz` starting with digits.
  2. **Numeric Evaluation:** Calls `parseNumericLiteral()`. Digits `12345` are read in base 10, but `x`, `y`, `z` fail decimal conversion.
  3. **Error Triggered:** Emits `Lexical Error at line 3, column 14: invalid immediate '12345xyz'`.
  4. **Pipeline Halting:** Pipeline stops with exit code 1.

---

### Test 063: Reserved Register Keyword as Label
* **Source Code:**
  ```assembly
  section .text
  eax:
      nop
  ```
* **Worklet 1 Failure Workflow:**
  1. **Lexical Scanning:** Identifies token `eax` followed by a colon `:`.
  2. **Reserved Word Validation:** `isValidRegisterName("EAX")` returns `true`.
  3. **Error Triggered:** Emits `Lexical Error at line 2, column 1: label name 'eax' is a reserved word`.
  4. **Pipeline Halting:** Halts immediately. Hardware registers cannot be redefined as user labels.

---

### Test 064: Unsupported / Unknown Mnemonic
* **Source Code:**
  ```assembly
  section .text
  _start:
      invalidop eax, ebx
  ```
* **Worklet 1 Failure Workflow:**
  1. **Mnemonic Lookup:** `parseAssemblySource()` queries `lookupBaseMnemonic("INVALIDOP")` and `lookupExtendedMnemonic()`. Both return `false`.
  2. **Error Triggered:** Emits `Syntax Error at line 3, column 5: unsupported mnemonic 'invalidop'`.
  3. **Pipeline Halting:** Front-end parser stops before IR creation.

---

### Test 065: 16-bit Register in Memory Brackets
* **Source Code:**
  ```assembly
  section .text
  _start:
      mov eax, [bx]
  ```
* **Worklet 1 Failure Workflow:**
  1. **Memory Evaluation:** `parseMemoryExpressionComponents()` extracts base register `BX`.
  2. **Bit-Width Check:** `lookupRegister("BX", r, bits)` returns `bits = 16`.
  3. **Error Triggered:** Emits `Syntax Error at line 3, column 14: 16/8-bit register 'BX' cannot be used as an address (use 32-bit)`.
  4. **Pipeline Halting:** Protected-mode 32-bit addressing rejects 16-bit address registers.

---

### Test 066: Short Branch (LOOP) to External Symbol
* **Source Code:**
  ```assembly
  extern ext_func
  section .text
  _start:
      loop ext_func
  ```
* **Worklet 1 Failure Workflow:**
  1. **Operand Categorization:** `loop` targets external symbol `ext_func`.
  2. **Branch Check:** `isShortBranchInstruction(w2::Op::LOOP)` returns `true`.
  3. **Relocation Constraint:** A rel8 branch (-128..+127 bytes) cannot hold an external 32-bit relocation.
  4. **Error Triggered:** Emits `Syntax Error at line 4, column 10: LOOP cannot reference an external symbol (rel8)`. Pipeline terminates.

---

### Test 067: Register Subtraction in Memory Operand
* **Source Code:**
  ```assembly
  section .text
  _start:
      mov eax, [ebx-ecx]
  ```
* **Worklet 1 Failure Workflow:**
  1. **Syntax Validator:** `validateMemoryExpressionSyntax("[ebx-ecx]")` scans mathematical operators between registers.
  2. **Hardware Constraint:** ModR/M and SIB hardware physically only adds registers: `Base + Index*Scale + Disp`.
  3. **Error Triggered:** Emits `Syntax Error at line 3, column 14: a register cannot be subtracted in a memory operand ('[ebx-ecx]')`.

---

### Test 068: Missing Operand After Comma
* **Source Code:**
  ```assembly
  section .text
  _start:
      mov eax,
  ```
* **Worklet 1 Failure Workflow:**
  1. **Operand Loop:** Consumes `eax`, then reads comma `,`.
  2. **Trailing Token Check:** Index advances to end-of-line tokens.
  3. **Error Triggered:** Emits `Syntax Error at line 3, column 13: missing operand after comma`. Pipeline halts.

---

### Test 069: Undefined Symbol Reference
* **Source Code:**
  ```assembly
  section .text
  _start:
      jmp nonexistent_target
  ```
* **Worklet 1 Failure Workflow:**
  1. **Symbol Resolution:** Evaluates identifier `nonexistent_target`.
  2. **Table Search:** Checks `idByName` (local labels) and `externs` set. Symbol is absent from both.
  3. **Error Triggered:** Emits `Symbol Error at line 3, column 9: undefined symbol 'nonexistent_target'`.

---

### Test 070: Declared GLOBAL Symbol Never Defined
* **Source Code:**
  ```assembly
  global undefined_global_func

  section .text
  _start:
      nop
  ```
* **Worklet 1 Failure Workflow:**
  1. **Phase 2 Ingestion:** Source parses `nop` without syntax error.
  2. **Phase 4 Symbol Table Check:** `buildSymbolTable()` iterates through `srcSyms.globals`.
  3. **Definition Validation:** Asserts that `undefined_global_func` exists in `srcSyms.labels` or `srcSyms.equConstants`.
  4. **Error Triggered:** Emits `Symbol Table Error: Symbol Error at line 1: Global symbol 'undefined_global_func' is declared but not defined`. Pipeline halts.

---

## 4. Master Comparative Summary Table (30 Test Cases)

| Test ID | Test Case Title | Worklet 1 Pipeline Stage | Primary Operands / Error Point | Execution Result |
|:---:|---|---|---|:---:|
| **001** | MOV between registers (32/16/8-bit) | Phase 2 (IR Parser) | REG, REG (EAX..ESP, AX..SP, AL..BH) | **PASS** |
| **002** | MOV register with multi-radix immediates | Phase 2 (Numeric Parser) | REG, IMM (dec, hex, bin, oct, neg) | **PASS** |
| **003** | Memory addressing [base] and [base+disp] | Phase 2 (Memory Parser) | REG, MEM ([ebx], [ebp], [ecx-8]) | **PASS** |
| **004** | SIB memory addressing | Phase 2 (Memory Parser) | REG, MEM ([base + index*scale + disp]) | **PASS** |
| **005** | Index*scale + disp and absolute disp | Phase 2 (Memory Parser) | REG, MEM ([ecx*4+16], [0x1000]) | **PASS** |
| **006** | Sized memory operands (BYTE/WORD/DWORD) | Phase 2 (IR Parser) | MEM, IMM (explicit width directives) | **PASS** |
| **007** | XCHG across all operand combinations | Phase 2 (IR Parser) | REG, REG / REG, MEM (w2::Op::XCHG) | **PASS** |
| **008** | PUSH and POP with 32-bit & 16-bit regs | Phase 2 (IR Parser) | REG (single operand arity) | **PASS** |
| **009** | PUSH and POP with memory operands | Phase 2 (IR Parser) | MEM (explicit size directives) | **PASS** |
| **010** | Stack state (PUSHAD, POPAD, PUSHA, POPA) | Phase 2 (Lowering) | None (lowered to PUSHAD/POPAD prefix 0x66) | **PASS** |
| **011** | LEA simple, SIB, and label addressing | Phase 2 (IR Parser) | REG, MEM / MEMLABEL (REF_ABS_DISP) | **PASS** |
| **012** | LOAD and STORE pseudo-instructions | Phase 2 (Lowering) | Lowered to w2::Op::MOV | **PASS** |
| **013** | Accumulator conversions & flag transfers | Phase 2 (IR Parser) | None (CBW, CWDE, CWD, CDQ, LAHF, SAHF) | **PASS** |
| **014** | ADD arithmetic across 32, 16, 8-bit | Phase 2 (ALU Parser) | REG, REG / REG, IMM / MEM, REG | **PASS** |
| **015** | ADC (Add with Carry) across operand shapes | Phase 2 (ALU Parser) | REG, REG / REG, MEM / MEM, IMM | **PASS** |
| **016** | SUB arithmetic across all shapes | Phase 2 (ALU Parser) | REG, REG / REG, IMM / MEM, IMM | **PASS** |
| **017** | SBB (Subtract with Borrow) | Phase 2 (ALU Parser) | REG, REG / REG, MEM / MEM, IMM | **PASS** |
| **018** | CMP comparisons across sizes | Phase 2 (ALU Parser) | Flags-only subtraction evaluation | **PASS** |
| **019** | AND bitwise logic across sizes | Phase 2 (ALU Parser) | Bitmask evaluation & register pairs | **PASS** |
| **020** | OR bitwise logic across sizes | Phase 2 (ALU Parser) | 32-bit bitmasks & register transfers | **PASS** |
| **061** | Missing closing bracket in memory operand | Phase 2 (Lexer) | Rejected at line 3, col 14 (unclosed bracket) | **CAUGHT** |
| **062** | Invalid immediate literal format | Phase 2 (Lexer) | Rejected at line 3, col 14 (`12345xyz`) | **CAUGHT** |
| **063** | Reserved register keyword as label | Phase 2 (Lexer) | Rejected at line 2, col 1 (`eax:`) | **CAUGHT** |
| **064** | Unsupported / unknown mnemonic | Phase 2 (Parser) | Rejected at line 3, col 5 (`invalidop`) | **CAUGHT** |
| **065** | 16-bit register in memory operand | Phase 2 (Parser) | Rejected at line 3, col 14 (`[bx]`) | **CAUGHT** |
| **066** | Short branch (LOOP) to extern symbol | Phase 2 (Parser) | Rejected at line 4, col 10 (rel8 to extern) | **CAUGHT** |
| **067** | Register subtracted in memory operand | Phase 2 (Memory) | Rejected at line 3, col 14 (`[ebx-ecx]`) | **CAUGHT** |
| **068** | Missing operand after comma | Phase 2 (Parser) | Rejected at line 3, col 13 (`mov eax,`) | **CAUGHT** |
| **069** | Undefined symbol reference | Phase 2 (Symbols) | Rejected at line 3, col 9 (`nonexistent_target`) | **CAUGHT** |
| **070** | Declared GLOBAL symbol never defined | Phase 4 (Symbol Table) | Rejected at line 1 (`undefined_global_func`) | **CAUGHT** |

