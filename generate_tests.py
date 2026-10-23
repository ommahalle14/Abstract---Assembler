#!/usr/bin/env python3
"""
generate_tests.py
Generates 100 comprehensive .asm test cases for the x86-32 modular assembler.
- Tests 001 - 060: 60 Valid assembly test cases covering all operands, instructions, addressing modes, and directives.
- Tests 061 - 100: 40 Error test cases covering 40 distinct error conditions across Worklet 1, Worklet 2, and Worklet 3.
"""

import os

os.makedirs("tests", exist_ok=True)

test_files = {}

# ==============================================================================
# 60 VALID TEST CASES (001 - 060)
# ==============================================================================

test_files["test_001.asm"] = """; Test 001: MOV between registers (32-bit, 16-bit, and 8-bit)
section .text
global _start
_start:
    ; 32-bit register to register
    mov eax, ebx
    mov ecx, edx
    mov esi, edi
    mov ebp, esp

    ; 16-bit register to register
    mov ax, bx
    mov cx, dx
    mov si, di
    mov bp, sp

    ; 8-bit register to register (low and high)
    mov al, bl
    mov cl, dl
    mov ah, bh
    mov ch, dh
    mov al, ah
    mov bl, ch
    ret
"""

test_files["test_002.asm"] = """; Test 002: MOV register with all immediate formats (decimal, hex, binary, octal, negative)
section .text
global _start
_start:
    ; Decimal immediates
    mov eax, 100
    mov ebx, -42
    mov ecx, 255d

    ; Hexadecimal formats (0x and 'h' suffix)
    mov edx, 0x12345678
    mov esi, 0FFH
    mov edi, 0x7FFFFFFF

    ; Binary format ('b' suffix)
    mov al, 10101010b
    mov ah, 11110000b

    ; Octal format ('o' and 'q' suffix)
    mov cl, 77o
    mov ch, 177q

    ; Sized 16-bit immediates
    mov ax, 0x1234
    mov bx, -300
    ret
"""

test_files["test_003.asm"] = """; Test 003: Memory addressing with [base] and [base + disp]
section .text
global _start
_start:
    ; Simple base register addressing
    mov eax, [ebx]
    mov ecx, [esi]
    mov edx, [edi]
    mov ebx, [ebp]
    mov esi, [esp]

    ; Base + positive displacement (disp8 and disp32)
    mov eax, [ebx + 4]
    mov ecx, [esi + 128]
    mov edx, [edi + 0x1000]

    ; Base + negative displacement
    mov eax, [ebp - 4]
    mov ebx, [ebp - 8]
    mov ecx, [esp + 16]
    ret
"""

test_files["test_004.asm"] = """; Test 004: SIB memory addressing [base + index*scale + disp]
section .text
global _start
_start:
    ; Base + index (scale 1 implicit or explicit)
    mov eax, [ebx + ecx]
    mov edx, [esi + edi*1]

    ; Base + index with scale factors: 2, 4, 8
    mov eax, [ebx + edx*2]
    mov ecx, [esi + edi*4]
    mov edx, [ebp + eax*8]

    ; Base + index*scale + displacement
    mov eax, [ebx + ecx*4 + 16]
    mov ebx, [esi + edx*8 + 0x20]
    mov edx, [ebp + edi*2 - 8]
    mov ecx, [esp + eax*4 + 12]
    ret
"""

test_files["test_005.asm"] = """; Test 005: Index*scale + disp and absolute 32-bit displacement
section .text
global _start
_start:
    ; [index*scale] without base
    mov eax, [ecx*2]
    mov ebx, [edx*4]
    mov esi, [edi*8]

    ; [index*scale + disp]
    mov eax, [ecx*2 + 8]
    mov ebx, [edx*4 + 0x100]
    mov edx, [esi*8 - 16]

    ; [disp32] absolute address
    mov eax, [0]
    mov ebx, [0x1000]
    mov ecx, [0x401000]
    ret
"""

test_files["test_006.asm"] = """; Test 006: Sized memory operands (BYTE PTR, WORD PTR, DWORD PTR)
section .text
global _start
_start:
    ; DWORD PTR
    mov dword ptr [ebx], 5
    mov dword ptr [ebx + 4], eax
    mov eax, dword ptr [ebx + 8]

    ; WORD PTR
    mov word ptr [ecx], 0x1234
    mov word ptr [ecx + 2], ax
    mov ax, word ptr [ecx + 4]

    ; BYTE PTR
    mov byte ptr [edx], 0xFF
    mov byte ptr [edx + 1], al
    mov al, byte ptr [edx + 2]
    ret
"""

test_files["test_007.asm"] = """; Test 007: XCHG across all operand combinations
section .text
global _start
_start:
    ; Accumulator short form (90+r)
    xchg eax, ebx
    xchg ecx, eax
    xchg edx, eax
    xchg ax, bx
    xchg cx, ax

    ; General register to register
    xchg ebx, ecx
    xchg esi, edi
    xchg bl, cl
    xchg ah, bh

    ; Register with memory
    xchg eax, dword ptr [ebx]
    xchg ax, word ptr [ecx]
    xchg al, byte ptr [edx]
    xchg dword ptr [esi + 4], edi
    ret
"""

test_files["test_008.asm"] = """; Test 008: PUSH and POP with all 32-bit and 16-bit registers
section .text
global _start
_start:
    ; 32-bit register PUSH and POP
    push eax
    push ecx
    push edx
    push ebx
    push esp
    push ebp
    push esi
    push edi

    pop edi
    pop esi
    pop ebp
    pop esp
    pop ebx
    pop edx
    pop ecx
    pop eax

    ; 16-bit register PUSH and POP
    push ax
    push cx
    push dx
    push bx
    push sp
    push bp
    push si
    push di

    pop di
    pop si
    pop bp
    pop sp
    pop bx
    pop dx
    pop cx
    pop ax
    ret
"""

test_files["test_009.asm"] = """; Test 009: PUSH and POP with memory operands
section .text
global _start
_start:
    ; 32-bit memory operands
    push dword ptr [ebx]
    push dword ptr [ebp - 4]
    push dword ptr [esi + edi*4]
    pop dword ptr [esi + edi*4]
    pop dword ptr [ebp - 4]
    pop dword ptr [ebx]

    ; 16-bit memory operands
    push word ptr [ecx]
    push word ptr [edx + 2]
    pop word ptr [edx + 2]
    pop word ptr [ecx]
    ret
"""

test_files["test_010.asm"] = """; Test 010: Stack state manipulation (PUSHAD, POPAD, PUSHA, POPA, PUSHFD, POPFD, PUSHF, POPF)
section .text
global _start
_start:
    ; 32-bit stack instructions
    pushad
    popad
    pushfd
    popfd

    ; 16-bit stack instructions (0x66 prefix)
    pusha
    popa
    pushf
    popf
    ret
"""

test_files["test_011.asm"] = """; Test 011: LEA instruction with simple, SIB, and label addressing
section .text
global _start
_start:
    ; 32-bit LEA
    lea eax, [ebx + 4]
    lea ecx, [esi + edi*4 + 8]
    lea edx, [ebp - 16]
    lea ebx, [ecx*8 + 0x100]

    ; 16-bit LEA
    lea ax, [ebx + 4]
    lea bx, [esi + edi*2]

    ; LEA with bare label
    lea eax, my_target
my_target:
    nop
    ret
"""

test_files["test_012.asm"] = """; Test 012: LOAD and STORE pseudo-instructions
section .text
global _start
_start:
    ; LOAD reg, reg  -> MOV reg, [reg]
    load eax, ebx
    load ecx, esi

    ; LOAD reg, [mem] -> MOV reg, [mem]
    load edx, [ebp - 4]
    load eax, [ebx + ecx*4]

    ; LOAD reg, imm -> MOV reg, imm
    load eax, 100
    load ebx, 0x500

    ; STORE reg, reg -> MOV [reg], reg
    store ebx, eax
    store edi, ecx

    ; STORE [mem], reg -> MOV [mem], reg
    store [ebp - 8], edx
    store [esi + 4], eax
    ret
"""

test_files["test_013.asm"] = """; Test 013: Accumulator conversions and flag transfers (CBW, CWD, CWDE, CDQ, LAHF, SAHF)
section .text
global _start
_start:
    ; Sign conversions
    cbw         ; AL -> AX
    cwd         ; AX -> DX:AX
    cwde        ; AX -> EAX
    cdq         ; EAX -> EDX:EAX

    ; Flag transfers
    lahf        ; AH <- flags
    sahf        ; flags <- AH
    ret
"""

test_files["test_014.asm"] = """; Test 014: ADD arithmetic across 32, 16, and 8-bit operands
section .text
global _start
_start:
    ; 32-bit ADD
    add eax, ebx
    add ecx, 10
    add edx, 0x12345678
    add eax, [ebx]
    add [ebx + 4], eax
    add dword ptr [esi], 5

    ; 16-bit ADD
    add ax, bx
    add cx, 20
    add dx, [ebx]
    add [ecx + 2], ax
    add word ptr [edi], 1

    ; 8-bit ADD
    add al, bl
    add ah, 1
    add cl, [edx]
    add [esi], al
    add byte ptr [edi], 3
    ret
"""

test_files["test_015.asm"] = """; Test 015: ADC (Add with Carry) across all operand shapes
section .text
global _start
_start:
    ; 32-bit ADC
    adc eax, ebx
    adc ecx, 1
    adc edx, [ebx + 8]
    adc [esi + 4], eax
    adc dword ptr [edi], 10

    ; 16-bit ADC
    adc ax, bx
    adc cx, 5
    adc dx, [esi]
    adc [edi], ax

    ; 8-bit ADC
    adc al, bl
    adc ah, 1
    adc byte ptr [ebx], 0xFF
    ret
"""

test_files["test_016.asm"] = """; Test 016: SUB arithmetic across registers, memory, and immediates
section .text
global _start
_start:
    ; 32-bit SUB
    sub eax, ebx
    sub ecx, 100
    sub edx, [esi]
    sub [edi + 4], eax
    sub dword ptr [ebp - 4], 42

    ; 16-bit SUB
    sub ax, bx
    sub cx, 15
    sub dx, [ebx]
    sub word ptr [esi], 3

    ; 8-bit SUB
    sub al, cl
    sub ah, 5
    sub byte ptr [edx], 1
    ret
"""

test_files["test_017.asm"] = """; Test 017: SBB (Subtract with Borrow) across all operand forms
section .text
global _start
_start:
    ; 32-bit SBB
    sbb eax, ebx
    sbb ecx, 1
    sbb edx, [esi + 4]
    sbb [edi], eax
    sbb dword ptr [ebp - 8], 100

    ; 16-bit SBB
    sbb ax, bx
    sbb cx, 2
    sbb word ptr [ebx], 5

    ; 8-bit SBB
    sbb al, bl
    sbb ah, 1
    sbb byte ptr [edx], 7
    ret
"""

test_files["test_018.asm"] = """; Test 018: CMP comparisons across 32, 16, and 8-bit registers and memory
section .text
global _start
_start:
    ; 32-bit CMP
    cmp eax, ebx
    cmp ecx, 0
    cmp edx, 0x100
    cmp eax, [esi]
    cmp [edi + 4], edx
    cmp dword ptr [ebp - 4], 50

    ; 16-bit CMP
    cmp ax, bx
    cmp cx, 10
    cmp word ptr [ebx], 20

    ; 8-bit CMP
    cmp al, bl
    cmp ah, 0
    cmp byte ptr [edx], 0x55
    ret
"""

test_files["test_019.asm"] = """; Test 019: AND bitwise logic across all sizes and operand types
section .text
global _start
_start:
    ; 32-bit AND
    and eax, ebx
    and ecx, 0xFF
    and edx, 0x12345678
    and eax, [esi]
    and [edi], ebx
    and dword ptr [ebx + 4], 0x0F

    ; 16-bit AND
    and ax, bx
    and cx, 0x0F0F
    and word ptr [esi], 0xAA

    ; 8-bit AND
    and al, bl
    and ah, 0x7F
    and byte ptr [edx], 0x01
    ret
"""

test_files["test_020.asm"] = """; Test 020: OR bitwise logic across all sizes
section .text
global _start
_start:
    ; 32-bit OR
    or eax, ebx
    or ecx, 1
    or edx, 0x80000000
    or eax, [esi]
    or [edi + 8], ebx
    or dword ptr [ebx], 0xFF

    ; 16-bit OR
    or ax, bx
    or cx, 0x8000
    or word ptr [esi], 0x12

    ; 8-bit OR
    or al, bl
    or ah, 0x80
    or byte ptr [edx], 0x02
    ret
"""

test_files["test_021.asm"] = """; Test 021: XOR bitwise logic and register clearing
section .text
global _start
_start:
    ; Clearing registers with XOR
    xor eax, eax
    xor ecx, ecx
    xor edx, edx
    xor ebx, ebx
    xor esi, esi
    xor edi, edi

    ; XOR with memory and immediate
    xor eax, [ebx + 4]
    xor [esi], edx
    xor dword ptr [edi], 0xAA55AA55

    ; 16-bit and 8-bit XOR
    xor ax, bx
    xor word ptr [ebx], 0xFFFF
    xor al, bl
    xor byte ptr [edx], 0x0F
    ret
"""

test_files["test_022.asm"] = """; Test 022: TEST non-destructive logical comparison
section .text
global _start
_start:
    ; Testing registers
    test eax, eax
    test eax, ebx
    test ecx, 1
    test edx, 0x100

    ; Testing memory
    test dword ptr [ebx], 0x80
    test eax, [esi]
    test [edi], eax

    ; 16-bit and 8-bit TEST
    test ax, bx
    test cx, 0x01
    test word ptr [ebx], 0x10
    test al, al
    test ah, 0x80
    test byte ptr [edx], 0x01
    ret
"""

test_files["test_023.asm"] = """; Test 023: INC and DEC unary operations
section .text
global _start
_start:
    ; 32-bit INC / DEC (single-byte 40+r / 48+r opcodes)
    inc eax
    inc ecx
    inc edx
    inc ebx
    inc esp
    inc ebp
    inc esi
    inc edi

    dec edi
    dec esi
    dec ebp
    dec esp
    dec ebx
    dec edx
    dec ecx
    dec eax

    ; 16-bit INC / DEC
    inc ax
    inc cx
    dec dx
    dec bx

    ; 8-bit INC / DEC
    inc al
    inc ah
    dec bl
    dec bh

    ; Memory INC / DEC
    inc dword ptr [ebx]
    dec dword ptr [esi + 4]
    inc word ptr [ecx]
    dec byte ptr [edx]
    ret
"""

test_files["test_024.asm"] = """; Test 024: NOT and NEG unary operations
section .text
global _start
_start:
    ; 32-bit NOT and NEG
    not eax
    neg eax
    not ebx
    neg ecx

    ; 16-bit NOT and NEG
    not ax
    neg bx

    ; 8-bit NOT and NEG
    not al
    neg ah

    ; Memory operands
    not dword ptr [ebx]
    neg dword ptr [esi + 4]
    not word ptr [ecx]
    neg byte ptr [edx]
    ret
"""

test_files["test_025.asm"] = """; Test 025: MUL and DIV unsigned arithmetic
section .text
global _start
_start:
    ; 32-bit MUL and DIV
    mul ecx
    mul edx
    mul dword ptr [ebx]
    div esi
    div edi
    div dword ptr [ebp - 4]

    ; 16-bit MUL and DIV
    mul cx
    mul word ptr [ebx]
    div dx
    div word ptr [esi]

    ; 8-bit MUL and DIV
    mul cl
    mul byte ptr [edx]
    div bl
    div byte ptr [edi]
    ret
"""

test_files["test_026.asm"] = """; Test 026: IDIV signed division across registers and memory
section .text
global _start
_start:
    ; 32-bit IDIV
    cdq
    idiv ecx
    idiv ebx
    idiv dword ptr [esi]
    idiv dword ptr [ebp - 8]

    ; 16-bit IDIV
    cwd
    idiv cx
    idiv word ptr [ebx]

    ; 8-bit IDIV
    cbw
    idiv bl
    idiv byte ptr [edx]
    ret
"""

test_files["test_027.asm"] = """; Test 027: IMUL (1-operand, 2-operand, and 3-operand forms)
section .text
global _start
_start:
    ; 1-operand IMUL (register only)
    imul ecx
    imul edx
    imul ax
    imul bx
    imul al
    imul bl

    ; 2-operand IMUL: r, r
    imul eax, ebx
    imul ecx, edx
    imul ax, bx

    ; 3-operand IMUL: r, r, imm (imm8 and imm32)
    imul eax, ebx, 5
    imul ebx, ecx, 1000
    imul ax, bx, 10

    ; 2-operand IMUL: r, mem
    imul eax, dword ptr [ebx]
    imul ecx, dword ptr [esi + 4]
    imul ax, word ptr [edx]
    ret
"""

test_files["test_028.asm"] = """; Test 028: SHL and SAL logical/arithmetic shift left
section .text
global _start
_start:
    ; SHL with 1 (short D1 opcode)
    shl eax, 1
    shl ebx, 1
    sal ecx, 1

    ; SHL / SAL with imm8 (2..31)
    shl eax, 2
    shl edx, 16
    sal esi, 31

    ; SHL / SAL with CL register count
    shl eax, cl
    sal ebx, cl

    ; Sized memory operands
    shl dword ptr [ebx], 1
    shl dword ptr [esi], 4
    shl dword ptr [edi], cl
    shl word ptr [ecx], 2
    shl byte ptr [edx], 1
    ret
"""

test_files["test_029.asm"] = """; Test 029: SHR logical shift right
section .text
global _start
_start:
    ; SHR with 1
    shr eax, 1
    shr ebx, 1

    ; SHR with imm8 (2..31)
    shr eax, 4
    shr ecx, 8
    shr edx, 31

    ; SHR with CL
    shr eax, cl
    shr ebx, cl

    ; Memory operands
    shr dword ptr [ebx], 1
    shr dword ptr [esi + 4], 8
    shr dword ptr [edi], cl
    shr word ptr [ecx], 4
    shr byte ptr [edx], 2
    ret
"""

test_files["test_030.asm"] = """; Test 030: SAR arithmetic (signed) shift right
section .text
global _start
_start:
    ; SAR with 1
    sar eax, 1
    sar ebx, 1

    ; SAR with imm8 (2..31)
    sar eax, 8
    sar ecx, 16
    sar edx, 31

    ; SAR with CL
    sar eax, cl
    sar ebx, cl

    ; Memory operands
    sar dword ptr [ebx], 1
    sar dword ptr [esi], 4
    sar dword ptr [edi], cl
    sar word ptr [ecx], 2
    sar byte ptr [edx], 1
    ret
"""

test_files["test_031.asm"] = """; Test 031: ROL and ROR bitwise rotates
section .text
global _start
_start:
    ; ROL with 1, imm8, and CL
    rol eax, 1
    rol ebx, 4
    rol ecx, cl
    rol dword ptr [esi], 1
    rol dword ptr [edi], cl

    ; ROR with 1, imm8, and CL
    ror eax, 1
    ror ebx, 8
    ror ecx, cl
    ror dword ptr [ebx], 2
    ror dword ptr [esi], cl

    ; 16-bit and 8-bit rotates
    rol ax, 1
    ror bx, 2
    rol al, cl
    ror bl, 1
    ret
"""

test_files["test_032.asm"] = """; Test 032: RCL and RCR rotate through carry flag
section .text
global _start
_start:
    ; RCL with 1, imm8, and CL
    rcl eax, 1
    rcl ebx, 5
    rcl ecx, cl
    rcl dword ptr [esi], 1
    rcl dword ptr [edi], cl

    ; RCR with 1, imm8, and CL
    rcr eax, 1
    rcr ebx, 7
    rcr ecx, cl
    rcr dword ptr [ebx], 3
    rcr dword ptr [esi], cl

    ; 16-bit and 8-bit RCL/RCR
    rcl ax, 1
    rcr bx, 1
    rcl al, cl
    rcr bl, 2
    ret
"""

test_files["test_033.asm"] = """; Test 033: Unconditional JMP (label, register, and memory)
section .text
global _start
_start:
    ; Near jump to forward label
    jmp forward_target

back_target:
    ; Register indirect jump
    jmp eax

forward_target:
    ; Memory indirect jump
    jmp dword ptr [ebx]
    jmp back_target
"""

test_files["test_034.asm"] = """; Test 034: Function CALL and RET
section .text
global _start
_start:
    call helper_func
    call eax
    call dword ptr [ebx + 4]
    ret

helper_func:
    xor eax, eax
    ret
"""

test_files["test_035.asm"] = """; Test 035: Equality conditional jumps (JE, JZ, JNE, JNZ)
section .text
global _start
_start:
    cmp eax, 0
    je target_zero
    jz target_zero

    cmp eax, ebx
    jne target_not_zero
    jnz target_not_zero

target_zero:
    nop

target_not_zero:
    ret
"""

test_files["test_036.asm"] = """; Test 036: Signed comparison conditional jumps (JL/JNGE, JGE/JNL, JLE/JNG, JG/JNLE)
section .text
global _start
_start:
    cmp eax, 10
    jl lbl_less
    jnge lbl_less

    jge lbl_ge
    jnl lbl_ge

    jle lbl_le
    jng lbl_le

    jg lbl_g
    jnle lbl_g

lbl_less:
lbl_ge:
lbl_le:
lbl_g:
    ret
"""

test_files["test_037.asm"] = """; Test 037: Unsigned comparison jumps (JB/JNAE/JC, JAE/JNB/JNC, JBE/JNA, JA/JNBE)
section .text
global _start
_start:
    cmp eax, 20
    jb lbl_below
    jnae lbl_below
    jc lbl_below

    jae lbl_ae
    jnb lbl_ae
    jnc lbl_ae

    jbe lbl_be
    jna lbl_be

    ja lbl_a
    jnbe lbl_a

lbl_below:
lbl_ae:
lbl_be:
lbl_a:
    ret
"""

test_files["test_038.asm"] = """; Test 038: Flag conditional jumps (JS, JNS, JO, JNO, JP/JPE, JNP/JPO)
section .text
global _start
_start:
    test eax, eax
    js lbl_sign
    jns lbl_nosign

    add eax, 1
    jo lbl_overflow
    jno lbl_nooverflow

    test al, 0xFF
    jp lbl_parity
    jpe lbl_parity
    jnp lbl_noparity
    jpo lbl_noparity

lbl_sign:
lbl_nosign:
lbl_overflow:
lbl_nooverflow:
lbl_parity:
lbl_noparity:
    ret
"""

test_files["test_039.asm"] = """; Test 039: LOOP, LOOPE, LOOPZ, LOOPNE, LOOPNZ loops
section .text
global _start
_start:
    mov ecx, 5
loop_start:
    dec eax
    loop loop_start

    mov ecx, 3
loop_e_start:
    cmp eax, 0
    loope loop_e_start
    loopz loop_e_start

    mov ecx, 4
loop_ne_start:
    cmp eax, 1
    loopne loop_ne_start
    loopnz loop_ne_start
    ret
"""

test_files["test_040.asm"] = """; Test 040: JECXZ short jump when ECX is zero
section .text
global _start
_start:
    jecxz ecx_is_zero
    dec ecx
    jecxz ecx_is_zero
    nop
ecx_is_zero:
    ret
"""

test_files["test_041.asm"] = """; Test 041: String LODS and STOS instructions
section .text
global _start
_start:
    ; Specific byte, word, and dword string instructions
    lodsb
    lodsw
    lodsd

    stosb
    stosw
    stosd

    ; Generic LODS with register/size operand
    lods al
    lods ax
    lods eax
    lods byte ptr [esi]
    lods word ptr [esi]
    lods dword ptr [esi]

    ; Generic STOS with register/size operand
    stos al
    stos ax
    stos eax
    stos byte ptr [edi]
    stos word ptr [edi]
    stos dword ptr [edi]
    ret
"""

test_files["test_042.asm"] = """; Test 042: String MOVS, CMPS, and SCAS instructions
section .text
global _start
_start:
    ; Move string
    movsb
    movsw
    movsd

    ; Compare string
    cmpsb
    cmpsw
    cmpsd

    ; Scan string
    scasb
    scasw
    scasd
    ret
"""

test_files["test_043.asm"] = """; Test 043: Flag operations and processor control (CLC, STC, CMC, CLD, STD, CLI, STI, WAIT, NOP, HLT)
section .text
global _start
_start:
    clc         ; Clear carry
    stc         ; Set carry
    cmc         ; Complement carry
    cld         ; Clear direction
    std         ; Set direction
    cli         ; Clear interrupts
    sti         ; Set interrupts
    wait        ; Wait for FPU
    nop         ; No-op
    hlt         ; Halt
    ret
"""

test_files["test_044.asm"] = """; Test 044: INT software interrupts
section .text
global _start
_start:
    int 0x80    ; Linux syscall
    int 21h     ; DOS interrupt
    int 3       ; Debug breakpoint (CD 03)
    int 0x10    ; Video BIOS
    ret
"""

test_files["test_045.asm"] = """; Test 045: LEAVE instruction for stack frame unwinding
section .text
global _start
_start:
    ; Standard function prologue
    push ebp
    mov ebp, esp
    sub esp, 16

    ; Function body
    mov dword ptr [ebp - 4], 42

    ; Epilogue using LEAVE
    leave
    ret
"""

test_files["test_046.asm"] = """; Test 046: EQU constant definitions and substitutions
CONST_A equ 10
CONST_B equ 0x20
CONST_C equ 100

section .text
global _start
_start:
    mov eax, CONST_A
    add eax, CONST_B
    cmp eax, CONST_C
    mov dword ptr [ebx + CONST_A], eax
    ret
"""

test_files["test_047.asm"] = """; Test 047: EQU with arithmetic expressions (+, -, *, /)
VAL_BASE  equ 10
VAL_ADD   equ VAL_BASE + 5
VAL_SUB   equ 20 - 4
VAL_MULT  equ 8 * 4
VAL_DIV   equ 100 / 2

section .text
global _start
_start:
    mov eax, VAL_ADD
    mov ebx, VAL_SUB
    mov ecx, VAL_MULT
    mov edx, VAL_DIV
    ret
"""

test_files["test_048.asm"] = """; Test 048: Multi-section program (.text and .data)
section .data
msg_byte:  db 0x55
msg_word:  dw 0x1234
msg_dword: dd 0xAABBCCDD

section .text
global _start
_start:
    mov al, byte ptr [msg_byte]
    mov ax, word ptr [msg_word]
    mov eax, dword ptr [msg_dword]
    ret
"""

test_files["test_049.asm"] = """; Test 049: Uninitialized data section (.bss) with RESB, RESW, RESD
section .bss
buffer: resb 64
array:  resw 16
var32:  resd 4

section .text
global _start
_start:
    mov dword ptr [var32], 123
    mov word ptr [array], 456
    mov byte ptr [buffer], 78
    ret
"""

test_files["test_050.asm"] = """; Test 050: String literals in .data (double and single quotes)
section .data
str_hello: db "Hello, World!", 0
str_quote: db 'Assembly language is fast', 10, 0

section .text
global _start
_start:
    lea esi, str_hello
    lea edi, str_quote
    lodsb
    ret
"""

test_files["test_051.asm"] = """; Test 051: TIMES repeat directive in .data section
section .data
zeros_byte:  times 10 db 0
ones_dword:  times 4 dd 0xFFFFFFFF
pattern:     times 8 dw 0xAAAA

section .text
global _start
_start:
    mov eax, dword ptr [zeros_byte]
    mov ebx, dword ptr [ones_dword]
    ret
"""

test_files["test_052.asm"] = """; Test 052: TIMES repeat directive in .bss section
section .bss
stack_space: times 32 resd 1
scratch_buf: times 128 resb 1

section .text
global _start
_start:
    mov dword ptr [stack_space], 0
    mov byte ptr [scratch_buf], 0
    ret
"""

test_files["test_053.asm"] = """; Test 053: GLOBAL symbols export
global _start
global my_exported_function
global my_exported_data

section .data
my_exported_data: dd 42

section .text
_start:
    call my_exported_function
    ret

my_exported_function:
    mov eax, dword ptr [my_exported_data]
    ret
"""

test_files["test_054.asm"] = """; Test 054: EXTERN symbol declarations and external calls
extern external_calc
extern external_data_table

section .text
global _start
_start:
    call external_calc
    mov eax, dword ptr [external_data_table]
    ret
"""

test_files["test_055.asm"] = """; Test 055: Data section relocations (pointers to code and data)
section .data
target_var: dd 100
ptr_var:    dd target_var
ptr_func:   dd my_func
ptr_offset: dd target_var + 4

section .text
global _start
_start:
    mov eax, dword ptr [ptr_var]
    call my_func
    ret

my_func:
    xor eax, eax
    ret
"""

test_files["test_056.asm"] = """; Test 056: Memory addressing referencing labels with displacement
section .data
data_table:
    dd 10, 20, 30, 40

section .text
global _start
_start:
    mov eax, [data_table]
    mov ebx, [data_table + 4]
    mov ecx, [data_table + 8]
    mov edx, [data_table + 12]
    ret
"""

test_files["test_057.asm"] = """; Test 057: Complex memory addressing [base + index*scale + label + disp]
section .data
array_base: dd 1, 2, 3, 4, 5, 6, 7, 8

section .text
global _start
_start:
    mov esi, 2
    mov eax, [ebx + esi*4 + array_base + 8]
    mov edx, [ebp + array_base]
    ret
"""

test_files["test_058.asm"] = """; Test 058: Register mix across 32, 16, and 8-bit sizes in pipeline
section .text
global _start
_start:
    mov eax, 0x11223344
    mov ax, 0x5566
    mov al, 0x77
    mov ah, 0x88

    mov ebx, 0x99AABBCC
    mov bx, 0xDDEE
    mov bl, 0xFF
    mov bh, 0x00

    mov ecx, eax
    mov cx, bx
    mov cl, al
    mov ch, bh
    ret
"""

test_files["test_059.asm"] = """; Test 059: Interwoven forward and backward jump sequences
section .text
global _start
_start:
    mov ecx, 3

block_a:
    inc eax
    jmp block_b

block_c:
    add ebx, 2
    dec ecx
    jnz block_a
    jmp done

block_b:
    mul edx
    jmp block_c

done:
    ret
"""

test_files["test_060.asm"] = """; Test 060: Comprehensive realistic pipeline program
global _start

section .data
msg:     db "Hello, x86-32!", 10
len equ  14

section .bss
res_buf: resb 32

section .text
_start:
    ; Function prologue
    push ebp
    mov ebp, esp

    ; Setup loop to copy msg into res_buf
    lea esi, msg
    lea edi, res_buf
    mov ecx, len

copy_loop:
    lodsb
    stosb
    loop copy_loop

    ; Write syscall: eax=4, ebx=1, ecx=res_buf, edx=len
    mov eax, 4
    mov ebx, 1
    lea ecx, res_buf
    mov edx, len
    int 0x80

    ; Exit syscall: eax=1, ebx=0
    leave
    mov eax, 1
    xor ebx, ebx
    int 0x80
"""

# ==============================================================================
# 40 ERROR TEST CASES (061 - 100)
# ==============================================================================

test_files["test_061.asm"] = """; Test 061 [ERROR]: Unknown section directive name
; Stage: Worklet 3 (splitSource)
; Expected Error: unknown section '.MY_CUSTOM_SECTION'
section .my_custom_section
_start:
    nop
"""

test_files["test_062.asm"] = """; Test 062 [ERROR]: Duplicate EQU definition
; Stage: Worklet 3 (splitSource)
; Expected Error: duplicate equ definition of 'BUFFER_SIZE'
BUFFER_SIZE equ 1024
BUFFER_SIZE equ 2048

section .text
_start:
    mov eax, BUFFER_SIZE
"""

test_files["test_063.asm"] = """; Test 063 [ERROR]: EQU definition conflicts with existing label in data
; Stage: Worklet 3 (splitSource)
; Expected Error: symbol 'func_entry' already defined as a label
section .data
func_entry: db 1

func_entry equ 42
"""

test_files["test_064.asm"] = """; Test 064 [ERROR]: Label conflicts with existing EQU constant
; Stage: Worklet 3 (splitSource)
; Expected Error: label 'MAX_COUNT' conflicts with existing equ
MAX_COUNT equ 100

section .text
MAX_COUNT:
    nop
"""

test_files["test_065.asm"] = """; Test 065 [ERROR]: Duplicate label definition in .data section
; Stage: Worklet 3 (splitSource)
; Expected Error: duplicate label 'my_data_var'
section .data
my_data_var: db 1
my_data_var: db 2

section .text
_start:
    nop
"""

test_files["test_066.asm"] = """; Test 066 [ERROR]: Unknown directive in data section
; Stage: Worklet 3 (splitSource)
; Expected Error: unknown directive 'align'
section .data
align 4

section .text
_start:
    nop
"""

test_files["test_067.asm"] = """; Test 067 [ERROR]: Malformed TIMES directive missing repeat count
; Stage: Worklet 3 (splitSource)
; Expected Error: malformed times directive
section .data
times db 10

section .text
_start:
    nop
"""

test_files["test_068.asm"] = """; Test 068 [ERROR]: Negative repeat count in TIMES directive
; Stage: Worklet 3 (splitSource)
; Expected Error: negative repeat count -5
section .data
times -5 db 1

section .text
_start:
    nop
"""

test_files["test_069.asm"] = """; Test 069 [ERROR]: TIMES repeat count exceeds maximum limit (16,000,000)
; Stage: Worklet 3 (splitSource)
; Expected Error: exceeds maximum limit of 16000000
section .data
times 20000000 db 1

section .text
_start:
    nop
"""

test_files["test_070.asm"] = """; Test 070 [ERROR]: RESB directive requires count operand
; Stage: Worklet 3 (splitSource)
; Expected Error: 'RESB' directive requires count operand
section .bss
buffer: resb

section .text
_start:
    nop
"""

test_files["test_071.asm"] = """; Test 071 [ERROR]: DB directive requires operands
; Stage: Worklet 3 (splitSource)
; Expected Error: 'DB' directive requires operands
section .data
empty_bytes: db

section .text
_start:
    nop
"""

test_files["test_072.asm"] = """; Test 072 [ERROR]: Empty operand in DB comma list
; Stage: Worklet 3 (splitSource)
; Expected Error: empty operand in 'DB' directive
section .data
bad_list: db 1, , 2

section .text
_start:
    nop
"""

test_files["test_073.asm"] = """; Test 073 [ERROR]: Value out of range for DB (-128..255)
; Stage: Worklet 3 (splitSource)
; Expected Error: value 500 out of range for db (-128..255)
section .data
overflow_byte: db 500

section .text
_start:
    nop
"""

test_files["test_074.asm"] = """; Test 074 [ERROR]: Value out of range for DW (-32768..65535)
; Stage: Worklet 3 (splitSource)
; Expected Error: value 100000 out of range for dw (-32768..65535)
section .data
overflow_word: dw 100000

section .text
_start:
    nop
"""

test_files["test_075.asm"] = """; Test 075 [ERROR]: Division by zero in EQU expression
; Stage: Worklet 3 (splitSource)
; Expected Error: division by zero
DIV_ZERO equ 100 / 0

section .text
_start:
    mov eax, DIV_ZERO
"""

test_files["test_076.asm"] = """; Test 076 [ERROR]: Lexical error: missing closing bracket ']' in memory operand
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... missing closing ']' for memory operand
section .text
_start:
    mov eax, [ebx + 4
"""

test_files["test_077.asm"] = """; Test 077 [ERROR]: Lexical error: invalid immediate literal
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... invalid immediate '12345xyz'
section .text
_start:
    mov eax, 12345xyz
"""

test_files["test_078.asm"] = """; Test 078 [ERROR]: Lexical error: invalid special character
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... invalid character '@'
section .text
_start:
    mov eax, @
"""

test_files["test_079.asm"] = """; Test 079 [ERROR]: Lexical error: label name is a reserved register keyword
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... label name 'eax' is a reserved word
section .text
eax:
    nop
"""

test_files["test_080.asm"] = """; Test 080 [ERROR]: Lexical error: label name is a reserved instruction mnemonic
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... label name 'mov' is a reserved word
section .text
mov:
    nop
"""

test_files["test_081.asm"] = """; Test 081 [ERROR]: Syntax error: expected a mnemonic, found number
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... expected a mnemonic, found '9999'
section .text
_start:
    9999 eax, ebx
"""

test_files["test_082.asm"] = """; Test 082 [ERROR]: Syntax error: unsupported / unknown mnemonic
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... unsupported mnemonic 'invalidop'
section .text
_start:
    invalidop eax, ebx
"""

test_files["test_083.asm"] = """; Test 083 [ERROR]: Syntax error: missing operand after size directive
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... missing operand after size directive
section .text
_start:
    mov eax, dword ptr
"""

test_files["test_084.asm"] = """; Test 084 [ERROR]: Syntax error: unknown register name
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... unknown register 'r15'
section .text
_start:
    mov eax, r15
"""

test_files["test_085.asm"] = """; Test 085 [ERROR]: Syntax error: 16-bit register inside memory brackets
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... 16/8-bit register 'BX' cannot be used as an address (use 32-bit)
section .text
_start:
    mov eax, [bx]
"""

test_files["test_086.asm"] = """; Test 086 [ERROR]: Symbol error: undefined symbol reference
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Symbol Error ... undefined symbol 'nonexistent_target'
section .text
_start:
    jmp nonexistent_target
"""

test_files["test_087.asm"] = """; Test 087 [ERROR]: Syntax error: rel8 branch (LOOP) cannot reference external symbol
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... LOOP cannot reference an external symbol (rel8)
extern ext_func
section .text
_start:
    loop ext_func
"""

test_files["test_088.asm"] = """; Test 088 [ERROR]: Syntax error: bare label cannot be used as an operand of MUL
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a label cannot be used as an operand of MUL
section .text
_start:
    mul target_label
target_label:
    nop
"""

test_files["test_089.asm"] = """; Test 089 [ERROR]: Syntax error: invalid scale multiplier in SIB memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... invalid memory expression '[ebx+ecx*3]'
section .text
_start:
    mov eax, [ebx+ecx*3]
"""

test_files["test_090.asm"] = """; Test 090 [ERROR]: Syntax error: ESP used as scaled index register
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... invalid memory expression '[ebx+esp*2]'
section .text
_start:
    mov eax, [ebx+esp*2]
"""

test_files["test_091.asm"] = """; Test 091 [ERROR]: Syntax error: register subtracted in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a register cannot be subtracted in a memory operand
section .text
_start:
    mov eax, [ebx-ecx]
"""

test_files["test_092.asm"] = """; Test 092 [ERROR]: Syntax error: scaled index register subtracted in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a scaled index register cannot be subtracted
section .text
_start:
    mov eax, [ebx-ecx*4]
"""

test_files["test_093.asm"] = """; Test 093 [ERROR]: Syntax error: at most two registers allowed in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... at most two registers (base + index) are allowed in a memory operand
section .text
_start:
    mov eax, [eax+ebx+ecx]
"""

test_files["test_094.asm"] = """; Test 094 [ERROR]: Syntax error: multiple labels in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... only one label is allowed in a memory operand
section .text
lbl_one: nop
lbl_two: nop
_start:
    mov eax, [lbl_one+lbl_two]
"""

test_files["test_095.asm"] = """; Test 095 [ERROR]: Syntax error: label cannot be subtracted in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a label cannot be subtracted in a memory operand
section .text
my_target: nop
_start:
    mov eax, [ebx-my_target]
"""

test_files["test_096.asm"] = """; Test 096 [ERROR]: Syntax error: expected comma between operands
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... expected comma
section .text
_start:
    mov eax ebx
"""

test_files["test_097.asm"] = """; Test 097 [ERROR]: Syntax error: missing operand after comma
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... missing operand after comma
section .text
_start:
    mov eax,
"""

test_files["test_098.asm"] = """; Test 098 [ERROR]: Syntax error: instruction has more than 3 operands
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... more than 3 operands
section .text
_start:
    mov eax, ebx, ecx, edx
"""

test_files["test_099.asm"] = """; Test 099 [ERROR]: Symbol error: duplicate label defined in .text
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Symbol Error ... duplicate symbol 'duplicate_label'
section .text
duplicate_label:
    nop
duplicate_label:
    nop
_start:
    ret
"""

test_files["test_100.asm"] = """; Test 100 [ERROR]: Symbol error: declared GLOBAL symbol is never defined
; Stage: Worklet 1 (buildSymbolTable)
; Expected Error: Symbol Error ... Global symbol 'undefined_global_func' is declared but not defined
global undefined_global_func

section .text
_start:
    nop
"""

# Write all 100 files
for filename, content in sorted(test_files.items()):
    filepath = os.path.join("tests", filename)
    with open(filepath, "w", newline="\n") as f:
        f.write(content)

print(f"Successfully generated {len(test_files)} test files in 'tests/' directory.")
