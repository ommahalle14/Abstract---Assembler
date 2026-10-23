; Test 069 [ERROR]: TIMES repeat count exceeds maximum limit (16,000,000)
; Stage: Worklet 3 (splitSource)
; Expected Error: exceeds maximum limit of 16000000
section .data
times 20000000 db 1

section .text
_start:
    nop
