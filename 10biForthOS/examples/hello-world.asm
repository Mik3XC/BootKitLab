    mov ax, 0x7e00
    mov es, ax
    xor bh, bh
    mov bp, msg

    mov ah, 0x13
    mov bl, 0x0A                ; foreground
    mov al, 1
    mov cx, [msg_length]
    mov dh, 11                  ; Y
    mov dl, 10                  ; X

    int 0x10


    jmp 0:0x7c00

msg db 'Hello, World!'
msg_length dw $-msg

