; Test 090 [ERROR]: Syntax error: ESP used as scaled index register
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... invalid memory expression '[ebx+esp*2]'
section .text
_start:
    mov eax, [ebx+esp*2]
