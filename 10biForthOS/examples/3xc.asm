; 3XC — prints the sequence, then returns to the boot loop.
; upload with:  ./send examples/3xc.asm
    mov ah,0x0E        ; BIOS teletype
    mov bh,0x00        ; page 0
    mov al,'3'
    int 0x10
    mov al,'X'
    int 0x10
    mov al,'C'
    int 0x10
    jmp 0:0x7c00       ; back to 10biForthOS
