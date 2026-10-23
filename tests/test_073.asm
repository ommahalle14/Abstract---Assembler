; Test 073 [ERROR]: Value out of range for DB (-128..255)
; Stage: Worklet 3 (splitSource)
; Expected Error: value 500 out of range for db (-128..255)
section .data
overflow_byte: db 500

section .text
_start:
    nop
