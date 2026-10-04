; Morpheus — a guided conversation over the serial wire for 10biForthOS.
; Prints the choice to the screen, then reads a keypress from COM1 via BIOS
; int 0x14 (the serial link). 'r' => red pill, 'b' => blue pill, anything
; else => a nudge, then keep waiting. Returns to the Forth banner on a choice.
; Loaded and run at 0x7e00:0 with DS=0x7e00 (same as the other examples).
    cld
    mov  si, intro
    call prints

.wait:
    call getchar              ; al = one byte from the serial port
    or   al, 0x20             ; crude tolower
    cmp  al, 'r'
    je   .red
    cmp  al, 'b'
    je   .blue
    mov  si, nudge
    call prints
    jmp  .wait

.red:
    mov  si, redmsg
    call prints
    jmp  0:0x7c00
.blue:
    mov  si, bluemsg
    call prints
    jmp  0:0x7c00

getchar:                      ; blocking read from COM1 (BIOS serial services)
    mov  ax, 0x0200
    xor  dx, dx
    int  0x14
    and  ah, 0x80             ; AH bit7 set => read failed/timeout
    jne  getchar              ; keep polling the wire
    ret

prints:                       ; teletype an asciiz string at DS:SI to the VGA
.p:
    lodsb
    test al, al
    jz   .r
    mov  ah, 0x0e
    mov  bx, 0x0007
    int  0x10
    jmp  .p
.r: ret

intro:
    db 13,10,"MORPHEUS: This is your last chance. After this, no turning back.",13,10
    db "  blue pill [b] -- the story ends.",13,10
    db "  red  pill [r] -- you stay in Wonderland.",13,10
    db "> ",0
nudge:
    db 13,10,"MORPHEUS: Choose. r or b.",13,10,"> ",0
redmsg:
    db 13,10,"All I'm offering is the truth. Nothing more.",13,10,0
bluemsg:
    db 13,10,"You wake up and believe whatever you want to believe.",13,10,0
