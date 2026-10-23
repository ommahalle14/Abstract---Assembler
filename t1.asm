start:
    mov eax, 5
    and eax, ebx
    and eax, 0xF0
    and ecx, 0x12345678
    or  edx, 1
    xor eax, eax
    test eax, ebx
    test eax, 0x100
    test ecx, 7
    xchg eax, ebx
    imul eax, ecx
    shl eax, 2
    shr ebx, 1
    sar ecx, 31
    not eax
    neg ebx
    mul ecx
    div esi
    idiv edi
    cdq
    pushad
    popad
    pushfd
    popfd
    clc
    stc
    cld
    std
    jl  start
    jge start
    jle fwd
    jg  fwd
    jb  start
    jae start
    jbe fwd
    ja  fwd
    js  fwd
    jns fwd
    jz  fwd
    jnz start
    int 0x80
    leave
fwd:
    hlt

;   memory operands + 8/16-bit registers 
    mov eax, dword ptr [ebx]
    mov eax, dword ptr [ebx+8]
    mov eax, dword ptr [ebp-4]
    mov eax, dword ptr [esp+4]
    mov eax, dword ptr [ebx+esi*4+12]
    mov dword ptr [ebx], 5
    add eax, dword ptr [ebx+4]
    add dword ptr [ebx+4], eax
    inc dword ptr [ebx]
    push dword ptr [ebx]
    mov al, byte ptr [ebx]
    mov ax, word ptr [ebx]
    mov al, bl
    add al, 5
    add ax, bx
