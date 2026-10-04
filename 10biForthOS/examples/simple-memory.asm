    mov al, [data]

    mov ah,0x0E
    mov bh,0x00
    int 0x10

    jmp 0:0x7c00


data:
    db 'A'
