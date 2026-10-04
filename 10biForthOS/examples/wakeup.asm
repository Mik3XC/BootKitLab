; Wake Up, Neo — Matrix boot intro for 10biForthOS
; Teletypes a message to the VGA screen with a typewriter delay, then returns.
; 0x1f in the string = dramatic pause (not printed). Runs at 0x7e00:0, DS=0x7e00.
    cld
    mov si, msg
.next:
    lodsb                     ; al = [ds:si], si++
    test al, al
    jz   .done
    cmp  al, 0x1f             ; pause sentinel?
    jne  .show
    mov  dx, 0x0040           ; long pause
    call delay
    jmp  .next
.show:
    mov  ah, 0x0e             ; BIOS teletype
    mov  bx, 0x0007
    int  0x10
    mov  dx, 0x0006           ; per-character pause
    call delay
    jmp  .next
.done:
    jmp  0:0x7c00

delay:                        ; dx = outer iterations
.d1:
    mov  cx, 0xffff
.d2:
    loop .d2
    dec  dx
    jnz  .d1
    ret

msg:
    db 13,10
    db "Wake up, Neo...", 13,10, 0x1f
    db "The Matrix has you...", 13,10, 0x1f
    db "Follow the white rabbit.", 13,10, 0x1f
    db "Knock, knock, Neo.", 13,10, 0
