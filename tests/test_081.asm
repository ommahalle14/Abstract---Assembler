; Test 081 [ERROR]: Syntax error: expected a mnemonic, found number
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... expected a mnemonic, found '9999'
section .text
_start:
    9999 eax, ebx
