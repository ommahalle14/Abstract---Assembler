; Test 026: IDIV signed division across registers and memory
section .text
global _start
_start:
    ; 32-bit IDIV
    cdq
    idiv ecx
    idiv ebx
    idiv dword ptr [esi]
    idiv dword ptr [ebp - 8]

    ; 16-bit IDIV
    cwd
    idiv cx
    idiv word ptr [ebx]

    ; 8-bit IDIV
    cbw
    idiv bl
    idiv byte ptr [edx]
    ret
