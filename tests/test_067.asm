; Test 067 [ERROR]: Malformed TIMES directive missing repeat count
; Stage: Worklet 3 (splitSource)
; Expected Error: malformed times directive
section .data
times db 10

section .text
_start:
    nop
