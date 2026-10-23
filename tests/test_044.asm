; Test 044: INT software interrupts
section .text
global _start
_start:
    int 0x80    ; Linux syscall
    int 21h     ; DOS interrupt
    int 3       ; Debug breakpoint (CD 03)
    int 0x10    ; Video BIOS
    ret
