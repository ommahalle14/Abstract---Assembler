; Test 009: PUSH and POP with memory operands
section .text
global _start
_start:
    ; 32-bit memory operands
    push dword ptr [ebx]
    push dword ptr [ebp - 4]
    push dword ptr [esi + edi*4]
    pop dword ptr [esi + edi*4]
    pop dword ptr [ebp - 4]
    pop dword ptr [ebx]

    ; 16-bit memory operands
    push word ptr [ecx]
    push word ptr [edx + 2]
    pop word ptr [edx + 2]
    pop word ptr [ecx]
    ret
