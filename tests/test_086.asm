; Test 086 [ERROR]: Symbol error: undefined symbol reference
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Symbol Error ... undefined symbol 'nonexistent_target'
section .text
_start:
    jmp nonexistent_target
