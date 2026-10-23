; Test 085 [ERROR]: Syntax error: 16-bit register inside memory brackets
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... 16/8-bit register 'BX' cannot be used as an address (use 32-bit)
section .text
_start:
    mov eax, [bx]
