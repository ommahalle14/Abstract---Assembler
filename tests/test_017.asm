; Test 017: SBB (Subtract with Borrow) across all operand forms
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
