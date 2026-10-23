; Test 011: LEA instruction with simple, SIB, and label addressing
section .text
global _start
_start:
    ; 32-bit LEA
    lea eax, [ebx + 4]
    lea ecx, [esi + edi*4 + 8]
    lea edx, [ebp - 16]
    lea ebx, [ecx*8 + 0x100]

    ; 16-bit LEA
    lea ax, [ebx + 4]
    lea bx, [esi + edi*2]

    ; LEA with bare label
    lea eax, my_target
my_target:
    nop
    ret
