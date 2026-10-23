; Test 039: LOOP, LOOPE, LOOPZ, LOOPNE, LOOPNZ loops
section .text
global _start
_start:
    mov ecx, 5
loop_start:
    dec eax
    loop loop_start

    mov ecx, 3
loop_e_start:
    cmp eax, 0
    loope loop_e_start
    loopz loop_e_start

    mov ecx, 4
loop_ne_start:
    cmp eax, 1
    loopne loop_ne_start
    loopnz loop_ne_start
    ret
