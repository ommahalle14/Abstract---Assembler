; Test 052: TIMES repeat directive in .bss section
section .bss
stack_space: times 32 resd 1
scratch_buf: times 128 resb 1

section .text
global _start
_start:
    mov dword ptr [stack_space], 0
    mov byte ptr [scratch_buf], 0
    ret
