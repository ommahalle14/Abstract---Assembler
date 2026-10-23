; Test 027: IMUL (1-operand, 2-operand, and 3-operand forms)
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
