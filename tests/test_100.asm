; Test 100 [ERROR]: Symbol error: declared GLOBAL symbol is never defined
; Stage: Worklet 1 (buildSymbolTable)
; Expected Error: Symbol Error ... Global symbol 'undefined_global_func' is declared but not defined
global undefined_global_func

section .text
_start:
    nop
