; Test 031: ROL and ROR bitwise rotates
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
