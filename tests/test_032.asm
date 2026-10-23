; Test 032: RCL and RCR rotate through carry flag
section .text
global _start
_start:
    ; RCL with 1, imm8, and CL
    rcl eax, 1
    rcl ebx, 5
    rcl ecx, cl
    rcl dword ptr [esi], 1
    rcl dword ptr [edi], cl

    ; RCR with 1, imm8, and CL
    rcr eax, 1
    rcr ebx, 7
    rcr ecx, cl
    rcr dword ptr [ebx], 3
    rcr dword ptr [esi], cl

    ; 16-bit and 8-bit RCL/RCR
    rcl ax, 1
    rcr bx, 1
    rcl al, cl
    rcr bl, 2
    ret
