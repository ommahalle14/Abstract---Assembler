; Test 074 [ERROR]: Value out of range for DW (-32768..65535)
; Stage: Worklet 3 (splitSource)
; Expected Error: value 100000 out of range for dw (-32768..65535)
section .data
overflow_word: dw 100000

section .text
_start:
    nop
