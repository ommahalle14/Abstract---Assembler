; Test 068 [ERROR]: Negative repeat count in TIMES directive
; Stage: Worklet 3 (splitSource)
; Expected Error: negative repeat count -5
section .data
times -5 db 1

section .text
_start:
    nop
