; Test 043: Flag operations and processor control (CLC, STC, CMC, CLD, STD, CLI, STI, WAIT, NOP, HLT)
section .text
global _start
_start:
    clc         ; Clear carry
    stc         ; Set carry
    cmc         ; Complement carry
    cld         ; Clear direction
    std         ; Set direction
    cli         ; Clear interrupts
    sti         ; Set interrupts
    wait        ; Wait for FPU
    nop         ; No-op
    hlt         ; Halt
    ret
