; Test 029: SHR logical shift right
section .text
global _start
_start:
    ; SHR with 1
    shr eax, 1
    shr ebx, 1

    ; SHR with imm8 (2..31)
    shr eax, 4
    shr ecx, 8
    shr edx, 31

    ; SHR with CL
    shr eax, cl
    shr ebx, cl

    ; Memory operands
    shr dword ptr [ebx], 1
    shr dword ptr [esi + 4], 8
    shr dword ptr [edi], cl
    shr word ptr [ecx], 4
    shr byte ptr [edx], 2
    ret
