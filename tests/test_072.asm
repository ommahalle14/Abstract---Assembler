; Test 072 [ERROR]: Empty operand in DB comma list
; Stage: Worklet 3 (splitSource)
; Expected Error: empty operand in 'DB' directive
section .data
bad_list: db 1, , 2

section .text
_start:
    nop
