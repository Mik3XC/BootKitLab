loop:
    call getchar

    cmp al,10
    je  newline

    cmp al,27
    jne .cont
    jmp 0:0x7c00

 .cont:
    call putchar

    jmp loop

getchar:                          ; get a char from serial (bios function)
    mov ax,0x0200
    xor dx,dx
    int 0x14

    and ah,0x80                  ; check for failure and clear ah as a side-effect
    jne getchar                  ; failed, try again later
    ret

putchar:
    mov ah,0x0E
    mov bh,0x00
    int 0x10
    ret

newline:
    mov  al,13
    call putchar
    mov  al,10
    call putchar
    jmp  loop
