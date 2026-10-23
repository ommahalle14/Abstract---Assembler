; Test 063 [ERROR]: EQU definition conflicts with existing label in data
; Stage: Worklet 3 (splitSource)
; Expected Error: symbol 'func_entry' already defined as a label
section .data
func_entry: db 1

func_entry equ 42
