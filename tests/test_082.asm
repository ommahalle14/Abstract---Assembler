; Test 082 [ERROR]: Syntax error: unsupported / unknown mnemonic
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... unsupported mnemonic 'invalidop'
section .text
_start:
    invalidop eax, ebx
