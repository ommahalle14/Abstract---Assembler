; Test 033: Unconditional JMP (label, register, and memory)
section .text
global _start
_start:
    ; Near jump to forward label
    jmp forward_target

back_target:
    ; Register indirect jump
    jmp eax

forward_target:
    ; Memory indirect jump
    jmp dword ptr [ebx]
    jmp back_target
