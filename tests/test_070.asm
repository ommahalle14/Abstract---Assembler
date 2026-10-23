; Test 070 [ERROR]: RESB directive requires count operand
; Stage: Worklet 3 (splitSource)
; Expected Error: 'RESB' directive requires count operand
section .bss
buffer: resb

section .text
_start:
    nop
