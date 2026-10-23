# Instruction Set Reference: x86-32 assembler (Worklet 1 + Worklet 2)

Generated from the code and from probing the built assembler: every "accepted" entry below was assembled successfully, and every example byte string is real output (`out.bin`). Jump examples use a label immediately after the instruction.

## 1. Summary

| Item | Count |
| --- | --- |
| Mnemonic names the assembler accepts (including aliases) | 114 |
| Distinct encoder operations (`w2::Op` values) | 93 |
| Operand sizes | 8-bit, 16-bit, 32-bit |
| General registers | 24 names: 8 × 32-bit, 8 × 16-bit, 8 × 8-bit |
| Conditional-jump names | 30 (16 distinct conditions) |
| No-operand instructions | 43 names |

**Legend.** `r` register, `m` memory operand, `i` immediate; sizes in bits. A dash means the form is rejected.

## 2. Registers, operands and syntax

| Kind | Supported |
|---|---|
| 32-bit registers | `EAX ECX EDX EBX ESP EBP ESI EDI` |
| 16-bit registers | `AX CX DX BX SP BP SI DI` |
| 8-bit registers | `AL CL DL BL AH CH DH BH` |
| Registers inside `[...]` | 32-bit only (16-bit addressing like `[bx+si]` is rejected) |
| Segment registers `CS DS ES FS GS SS` | Recognised by Worklet 1 but **not usable** as operands |
| Number formats | decimal `255`, `-1`, `255d`; hex `0xFF`, `0FFH` (must start with a digit); binary `1010b`; octal `17o` / `17q` |
| Size keywords | `BYTE PTR`, `WORD PTR`, `DWORD PTR` (`QWORD PTR` is parsed but rejected) |
| Labels | `name:` on its own line or before an instruction; forward and backward references; label at end of code |
| Comments / case | `;` comments; mnemonics and registers are case-insensitive; blank lines, tabs and CRLF are fine |

### Memory addressing forms (all encoded with the correct ModRM / SIB / disp8 / disp32)

| Form | Examples |
|---|---|
| `[base]` | `[EAX]` `[ESP]` `[EBP]` |
| `[base + disp]` | `[EBX+4]` `[ECX-128]` `[EDX+128]` `[EDI+12345678H]` |
| `[base + index]` | `[EAX+ECX]` `[EBP+EAX]` `[EAX+ESP]` (swapped to `[ESP+EAX]`) |
| `[base + index*scale (+ disp)]` | `[EBX+EDX*4]` `[ESI+EDI*8]` `[EAX+ECX*4+8]` (scale 1, 2, 4, 8) |
| `[index*scale + disp]` | `[ECX*2]` `[EDX*4+16]` `[EBX*8+1000]` |
| `[disp32]` (absolute) | `[0]` `[1000H]` `[0x12345678]` |
| `[label]` | `[table]`, address = base address + label offset |
| `[label + disp]` | `[table+4]` `[table-1]` |
| `[label + base + index*scale + disp]` | `[table+EBX*4+8]` `[EBP+table]` `[ESP+EAX*2+table+1]` |

ESP as index, subtracting a register, two scaled registers, two labels and scales other than 1/2/4/8 are rejected with a clear error.

## 3. Data movement

| Instruction | r,r | r,i | r,m | m,r | m,i | Example encodings |
|---|---|---|---|---|---|---|
| **MOV** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `8A C3`; `66 8B C3`; `8B C3`; `B9 05 00 00 00`; `8B 4B 04`; `C7 43 04 05 00 00 00` |
| **XCHG** | 8/16/32 | – | 8/16/32 | 8/16/32 | – | `86 D8`; `66 93`; `93`; `87 4B 04` |

Notes: `MOV m,i` without a size keyword defaults to 32-bit. `MOV r,imm` uses `B0+r` / `B8+r`. `XCHG` with AX/EAX uses the 1-byte `90+r` form. `XCHG` has no immediate form.

| Instruction | Forms | Examples |
|---|---|---|
| **PUSH** | r 16/32, m 16/32; **no immediate** | `50` `66 50` `FF 33` |
| **POP** | r 16/32, m 16/32 | `58` `66 58` `8F 03` |
| **LEA** | r 16/32 ← `[mem]` or a bare label (`LEA EAX, label`) | `8D 44 8B 08` (`LEA EAX,[EBX+ECX*4+8]`); `8D 05 06 00 00 00` (label); `66 8D 43 04` (16-bit) |
| **LOAD** (pseudo) | `LOAD r,reg` = `MOV r,[reg]`; `LOAD r,[mem]`; `LOAD r,label` = `MOV r,[label]`; `LOAD r,imm` = `MOV r,imm` | `8B 03`; `8B 43 04`; `8B 05 06 00 00 00`; `B8 05 00 00 00` |
| **STORE** (pseudo) | `STORE reg,r` = `MOV [reg],r`; `STORE [mem],r`; `STORE label,r` = `MOV [label],r` | `89 03`; `89 43 04`; `89 05 06 00 00 00` |

| Instruction | Operation | Bytes |
|---|---|---|
| **PUSHAD** | push all 32-bit registers | `60` |
| **POPAD** | pop all 32-bit registers | `61` |
| **PUSHA** | push all 16-bit registers (`66`-prefixed form) | `66 60` |
| **POPA** | pop all 16-bit registers (`66`-prefixed form) | `66 61` |
| **PUSHFD** | push EFLAGS | `9C` |
| **POPFD** | pop EFLAGS | `9D` |
| **PUSHF** | push FLAGS, 16-bit (`66`-prefixed form) | `66 9C` |
| **POPF** | pop FLAGS, 16-bit (`66`-prefixed form) | `66 9D` |
| **LAHF** | AH ← flags | `9F` |
| **SAHF** | flags ← AH | `9E` |
| **CBW** | AL → AX sign-extend | `66 98` |
| **CWD** | AX → DX:AX sign-extend | `66 99` |
| **CWDE** | AX → EAX sign-extend | `98` |
| **CDQ** | EAX → EDX:EAX sign-extend | `99` |

## 4. Arithmetic and logic

| Instruction | r,r | r,i | r,m | m,r | m,i | Example encodings (r8,r16,r32 / imm / mem / mem,imm) |
|---|---|---|---|---|---|---|
| **ADD** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `02 C3` ; `66 03 C3` ; `03 C3` ; `83 C1 05` ; `03 4B 04` ; `83 43 04 05` |
| **ADC** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `12 C3` ; `66 13 C3` ; `13 C3` ; `83 D1 05` ; `13 4B 04` ; `83 53 04 05` |
| **SUB** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `2A C3` ; `66 2B C3` ; `2B C3` ; `83 E9 05` ; `2B 4B 04` ; `83 6B 04 05` |
| **SBB** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `1A C3` ; `66 1B C3` ; `1B C3` ; `83 D9 05` ; `1B 4B 04` ; `83 5B 04 05` |
| **CMP** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `3A C3` ; `66 3B C3` ; `3B C3` ; `83 F9 05` ; `3B 4B 04` ; `83 7B 04 05` |
| **AND** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `22 C3` ; `66 23 C3` ; `23 C3` ; `83 E1 05` ; `23 4B 04` ; `83 63 04 05` |
| **OR** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `0A C3` ; `66 0B C3` ; `0B C3` ; `83 C9 05` ; `0B 4B 04` ; `83 4B 04 05` |
| **XOR** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `32 C3` ; `66 33 C3` ; `33 C3` ; `83 F1 05` ; `33 4B 04` ; `83 73 04 05` |
| **TEST** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `84 D8` ; `66 85 D8` ; `85 D8` ; `F7 C1 05 00 00 00` ; `85 4B 04` ; `F7 43 04 05 00 00 00` |

The encoder picks the shortest valid form: the 1-byte-immediate `83 /n` form when the value fits a signed byte, and the `AL/AX/EAX, imm` short opcodes. Immediates are range-checked for 8- and 16-bit operands.

| Instruction | Register | Memory | Examples (CL / CX / ECX / `DWORD PTR [EBX]`) |
|---|---|---|---|
| **INC** | 8/16/32 | 8/16/32 | `FE C1` ; `66 41` ; `41` ; `FF 03` |
| **DEC** | 8/16/32 | 8/16/32 | `FE C9` ; `66 49` ; `49` ; `FF 0B` |
| **NOT** | 8/16/32 | 8/16/32 | `F6 D1` ; `66 F7 D1` ; `F7 D1` ; `F7 13` |
| **NEG** | 8/16/32 | 8/16/32 | `F6 D9` ; `66 F7 D9` ; `F7 D9` ; `F7 1B` |
| **MUL** | 8/16/32 | 8/16/32 | `F6 E1` ; `66 F7 E1` ; `F7 E1` ; `F7 23` |
| **DIV** | 8/16/32 | 8/16/32 | `F6 F1` ; `66 F7 F1` ; `F7 F1` ; `F7 33` |
| **IDIV** | 8/16/32 | 8/16/32 | `F6 F9` ; `66 F7 F9` ; `F7 F9` ; `F7 3B` |
| **IMUL** (1-operand) | 8/16/32 | – (register only: Worklet 1 rejects memory) | `F6 E9` ; `66 F7 E9` ; `F7 E9` |
| **IMUL** (2-operand) | r,r 16/32; r,i 16/32 | r,m 16/32 | `0F AF C3` ; `6B C0 05` ; `69 C0 E8 03 00 00` ; `0F AF 03` |

`INC`/`DEC` on 16/32-bit registers use the 1-byte `40+r` / `48+r` forms. 2-operand `IMUL r,i` uses `6B` (imm8) or `69` (imm16/32).

## 5. Shifts and rotates

| Instruction | r,imm | r,CL | m,imm | m,CL | Examples (`EAX,1` ; `EAX,5` ; `EAX,CL` ; `DWORD PTR [EBX],3`) |
|---|---|---|---|---|---|
| **SHL** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 E0` ; `C1 E0 05` ; `D3 E0` ; `C1 23 03` |
| **SAL** (= SHL) | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 E0` ; `C1 E0 05` ; `D3 E0` ; `C1 23 03` |
| **SHR** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 E8` ; `C1 E8 05` ; `D3 E8` ; `C1 2B 03` |
| **SAR** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 F8` ; `C1 F8 05` ; `D3 F8` ; `C1 3B 03` |
| **ROL** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 C0` ; `C1 C0 05` ; `D3 C0` ; `C1 03 03` |
| **ROR** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 C8` ; `C1 C8 05` ; `D3 C8` ; `C1 0B 03` |
| **RCL** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 D0` ; `C1 D0 05` ; `D3 D0` ; `C1 13 03` |
| **RCR** | 8/16/32 | 8/16/32 | 8/16/32 | 8/16/32 | `D1 D8` ; `C1 D8 05` ; `D3 D8` ; `C1 1B 03` |

Immediate count is 0–31 (anything else is rejected). A count of 1 uses the 1-byte-shorter `D0/D1` form.

## 6. Control flow

| Instruction | Forms | Bytes |
|---|---|---|
| **JMP** | label (near, rel32); register 16/32; memory 16/32 | `E9 00 00 00 00` ; `FF E0` (`JMP EAX`) ; `FF 23` (`JMP DWORD PTR [EBX]`) |
| **CALL** | label (near, rel32); register 16/32; memory 16/32 | `E8 00 00 00 00` ; `FF D0` (`CALL EAX`) ; `FF 13` (`CALL DWORD PTR [EBX]`) |
| **RET** | near return, no operand | `C3` |
| **INT** | `INT imm8` (0–255) | `CD 21` (`INT 21H`) ; `CD 03` (`INT 3`; note: emits the 2-byte form, not `CC`) |
| **LEAVE** | `MOV ESP,EBP` / `POP EBP` | `C9` |
| **NOP / HLT** | no operand | `90` / `F4` |

### Conditional jumps (all near form, 6 bytes: `0F 8x` + rel32)

| Condition | Accepted names | Bytes |
|---|---|---|
| Equal / zero | `JE` `JZ` | `0F 84 00 00 00 00` |
| Not equal / not zero | `JNE` `JNZ` | `0F 85 00 00 00 00` |
| Less (signed) | `JL` `JNGE` | `0F 8C 00 00 00 00` |
| Greater or equal (signed) | `JGE` `JNL` | `0F 8D 00 00 00 00` |
| Less or equal (signed) | `JLE` `JNG` | `0F 8E 00 00 00 00` |
| Greater (signed) | `JG` `JNLE` | `0F 8F 00 00 00 00` |
| Below / carry (unsigned) | `JB` `JNAE` `JC` | `0F 82 00 00 00 00` |
| Above or equal / no carry | `JAE` `JNB` `JNC` | `0F 83 00 00 00 00` |
| Below or equal (unsigned) | `JBE` `JNA` | `0F 86 00 00 00 00` |
| Above (unsigned) | `JA` `JNBE` | `0F 87 00 00 00 00` |
| Sign | `JS` | `0F 88 00 00 00 00` |
| No sign | `JNS` | `0F 89 00 00 00 00` |
| Overflow | `JO` | `0F 80 00 00 00 00` |
| No overflow | `JNO` | `0F 81 00 00 00 00` |
| Parity even | `JP` `JPE` | `0F 8A 00 00 00 00` |
| Parity odd / no parity | `JNP` `JPO` | `0F 8B 00 00 00 00` |

### LOOP family and JECXZ (short jumps: opcode + rel8, target must be within −128…+127 bytes)

| Instruction | Meaning | Bytes |
|---|---|---|
| **LOOP** | ECX−1; jump if ≠ 0 | `E2 00` |
| **LOOPE / LOOPZ** | ECX−1; jump if ≠ 0 and ZF = 1 | `E1 00` |
| **LOOPNE / LOOPNZ** | ECX−1; jump if ≠ 0 and ZF = 0 | `E0 00` |
| **JECXZ** | jump if ECX = 0 | `E3 00` |

## 7. String instructions

| Instruction | 8-bit | 16-bit (`66` prefix) | 32-bit | Operation |
|---|---|---|---|---|
| **LODSB / LODSW / LODSD** | `LODSB` `AC` | `LODSW` `66 AD` | `LODSD` `AD` | load from `[ESI]` into AL/AX/EAX |
| **STOSB / STOSW / STOSD** | `STOSB` `AA` | `STOSW` `66 AB` | `STOSD` `AB` | store AL/AX/EAX to `[EDI]` |
| **MOVSB / MOVSW / MOVSD** | `MOVSB` `A4` | `MOVSW` `66 A5` | `MOVSD` `A5` | copy `[ESI]` → `[EDI]` |
| **CMPSB / CMPSW / CMPSD** | `CMPSB` `A6` | `CMPSW` `66 A7` | `CMPSD` `A7` | compare `[ESI]` with `[EDI]` |
| **SCASB / SCASW / SCASD** | `SCASB` `AE` | `SCASW` `66 AF` | `SCASD` `AF` | compare AL/AX/EAX with `[EDI]` |

`LODS` and `STOS` also accept one operand that only selects the size: `LODS AL|AX|EAX`, `LODS BYTE|WORD|DWORD PTR [ESI]`, and the same for `STOS` with `[EDI]` (no operand means 32-bit).

## 8. Flag and processor-control instructions

| Instruction | Operation | Byte |
|---|---|---|
| **CLC** | clear carry | `F8` |
| **STC** | set carry | `F9` |
| **CMC** | complement carry | `F5` |
| **CLD** | clear direction flag | `FC` |
| **STD** | set direction flag | `FD` |
| **CLI** | clear interrupt flag | `FA` |
| **STI** | set interrupt flag | `FB` |
| **WAIT** | wait for FPU | `9B` |
| **HLT** | halt | `F4` |
| **NOP** | no operation | `90` |

## 9. Complete alphabetical list of the 114 accepted mnemonic names

```
ADC     ADD     AND     CALL    CBW     CDQ     CLC     CLD     CLI     CMC     CMP
CMPSB   CMPSD   CMPSW   CWD     CWDE    DEC     DIV     HLT     IDIV    IMUL    INC
INT     JA      JAE     JB      JBE     JC      JE      JECXZ   JG      JGE     JL
JLE     JMP     JNA     JNAE    JNB     JNBE    JNC     JNE     JNG     JNGE    JNL
JNLE    JNO     JNP     JNS     JNZ     JO      JP      JPE     JPO     JS      JZ
LAHF    LEA     LEAVE   LOAD    LODS    LODSB   LODSD   LODSW   LOOP    LOOPE   LOOPNE
LOOPNZ  LOOPZ   MOV     MOVSB   MOVSD   MOVSW   MUL     NEG     NOP     NOT     OR
POP     POPA    POPAD   POPF    POPFD   PUSH    PUSHA   PUSHAD  PUSHF   PUSHFD  RCL
RCR     RET     ROL     ROR     SAHF    SAL     SAR     SBB     SCASB   SCASD   SCASW
SHL     SHR     STC     STD     STI     STORE   STOS    STOSB   STOSD   STOSW   SUB
TEST    WAIT    XCHG    XOR
```

Aliases (identical bytes, verified): `JZ=JE`, `JNZ=JNE`, `JNGE=JL`, `JNL=JGE`, `JNG=JLE`, `JNLE=JG`, `JNAE=JC=JB`, `JNB=JNC=JAE`, `JNA=JBE`, `JNBE=JA`, `JPE=JP`, `JPO=JNP`, `LOOPZ=LOOPE`, `LOOPNZ=LOOPNE`, `SAL=SHL`.

Pseudo-instructions (resolved by the adapter): `LOAD`, `STORE`, and the generic `LODS` / `STOS`.

## 10. Not supported

See the last section of `test_all_instructions.asm` and the gap list in the README. In short: data directives, expressions, `MOV reg,label` / `PUSH imm`, 16-bit addressing, segment registers and overrides, `REP`/`LOCK` prefixes, MOVZX/MOVSX, CMOVcc/SETcc, BT*/BSF/BSR/BSWAP/XADD/CMPXCHG, SHLD/SHRD, JCXZ, ENTER, RET imm16, INT3/INTO/IRET, IN/OUT, XLAT, BCD adjusts, x87/MMX/SSE/AVX and 64-bit mode.
