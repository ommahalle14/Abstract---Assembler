; Test 028: SHL and SAL logical/arithmetic shift left
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
