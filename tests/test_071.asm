; Test 071 [ERROR]: DB directive requires operands
; Stage: Worklet 3 (splitSource)
; Expected Error: 'DB' directive requires operands
section .data
empty_bytes: db

section .text
_start:
    nop
