; Test 059: Interwoven forward and backward jump sequences
section .text
global _start
_start:
    mov ecx, 3

block_a:
    inc eax
    jmp block_b

block_c:
    add ebx, 2
    dec ecx
    jnz block_a
    jmp done

block_b:
    mul edx
    jmp block_c

done:
    ret
