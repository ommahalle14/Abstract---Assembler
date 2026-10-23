; Test 064 [ERROR]: Label conflicts with existing EQU constant
; Stage: Worklet 3 (splitSource)
; Expected Error: label 'MAX_COUNT' conflicts with existing equ
MAX_COUNT equ 100

section .text
MAX_COUNT:
    nop
