loop:
    call getchar

    cmp al,13                 ; Newline
    je  newline

    cmp al,27                 ; Escape
    jne .cont
    jmp 0:0x7c00              ; return to REPL

 .cont:
    call putchar

    jmp loop

getchar:                      ; get char in al
    xor ax,ax
    int 0x16
    ret

putchar:                      ; put a char in al
    mov ah,0x0E
    mov bh,0x00
    int 0x10
    ret

newline:
    mov  al,13                ; Return
    call putchar
    mov  al,10                ; Linefeed
    call putchar
    jmp  loop
