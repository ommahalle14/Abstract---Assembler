; Test 014: ADD arithmetic across 32, 16, and 8-bit operands
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
