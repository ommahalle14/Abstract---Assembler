; Test 007: XCHG across all operand combinations
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
