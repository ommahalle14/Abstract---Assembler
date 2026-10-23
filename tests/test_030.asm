; Test 030: SAR arithmetic (signed) shift right
section .text
global _start
_start:
    ; SAR with 1
    sar eax, 1
    sar ebx, 1

    ; SAR with imm8 (2..31)
    sar eax, 8
    sar ecx, 16
    sar edx, 31

    ; SAR with CL
    sar eax, cl
    sar ebx, cl

    ; Memory operands
    sar dword ptr [ebx], 1
    sar dword ptr [esi], 4
    sar dword ptr [edi], cl
    sar word ptr [ecx], 2
    sar byte ptr [edx], 1
    ret
