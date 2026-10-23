; Test 012: LOAD and STORE pseudo-instructions
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
