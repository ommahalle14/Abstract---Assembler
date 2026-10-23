; Test 019: AND bitwise logic across all sizes and operand types
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
