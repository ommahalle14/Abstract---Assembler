; Test 035: Equality conditional jumps (JE, JZ, JNE, JNZ)
section .text
global _start
_start:
    cmp eax, 0
    je target_zero
    jz target_zero

    cmp eax, ebx
    jne target_not_zero
    jnz target_not_zero

target_zero:
    nop

target_not_zero:
    ret
