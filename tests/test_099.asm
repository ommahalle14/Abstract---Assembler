; Test 099 [ERROR]: Symbol error: duplicate label defined in .text
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Symbol Error ... duplicate symbol 'duplicate_label'
section .text
duplicate_label:
    nop
duplicate_label:
    nop
_start:
    ret
