; Test 040: JECXZ short jump when ECX is zero
section .text
global _start
_start:
    jecxz ecx_is_zero
    dec ecx
    jecxz ecx_is_zero
    nop
ecx_is_zero:
    ret
