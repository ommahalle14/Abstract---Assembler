; Test 016: SUB arithmetic across registers, memory, and immediates
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
