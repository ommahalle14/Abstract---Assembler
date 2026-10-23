; Test 066 [ERROR]: Unknown directive in data section
; Stage: Worklet 3 (splitSource)
; Expected Error: unknown directive 'align'
section .data
align 4

section .text
_start:
    nop
