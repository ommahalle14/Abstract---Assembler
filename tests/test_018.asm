; Test 018: CMP comparisons across 32, 16, and 8-bit registers and memory
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
