; Test 065 [ERROR]: Duplicate label definition in .data section
; Stage: Worklet 3 (splitSource)
; Expected Error: duplicate label 'my_data_var'
section .data
my_data_var: db 1
my_data_var: db 2

section .text
_start:
    nop
