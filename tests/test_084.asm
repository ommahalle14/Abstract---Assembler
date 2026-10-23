; Test 084 [ERROR]: Syntax error: unknown register name
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... unknown register 'r15'
section .text
_start:
    mov eax, r15
