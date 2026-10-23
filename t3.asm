main:
    mov ecx, 5
    mov esi, 0
    xor eax, eax
top:
    add al, byte ptr [ebx+esi*1]
    add word ptr [edi+ecx*2], 3
    inc esi
    dec ecx
    cmp ecx, 0
    jne top
    test al, al
    jz  done
    mov dword ptr [ebp-4], 1
done:
    push ax
    pop dx
    ret