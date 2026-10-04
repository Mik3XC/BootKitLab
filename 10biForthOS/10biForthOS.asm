    bits 16
    cpu 8086

    [org 0x7c00]                ; boot load address

EXECUTE equ 0x7e00              ; address where code should be compiled / executed

;;;--------------------
;;; Init
;;;--------------------
init:
    xor     ax,ax               ; DS=0 so the banner (org 0x7c00) is addressable by lodsb
    mov     ds,ax
    mov     si,banner
    call    print               ; show a boot splash on the VGA screen

    mov     ax,EXECUTE
    mov     ds,ax
    xor     di,di

;;;--------------------
;;; Interpreter loop
;;;--------------------
loop:
    call    getchar

    cmp     al,0
    jne     .skip
    jmp     EXECUTE:0x0
 .skip:

    cmp     al,1
    je      compile

    jmp     loop

;;;--------------------
;;; Compile
;;;--------------------
compile:
    call    getchar

    mov     byte [di],al
    inc     di

    jmp     loop

;;;--------------------
;;; Utils
;;;--------------------
getchar:                          ; get a char in al from serial (bios function)
    mov     ax,0x0200
    xor     dx,dx
    int     0x14

    and     ah,0x80               ; check for failure and clear ah as a side-effect
    jne     getchar               ; failed, try again
    ret

print:                            ; write a NUL-terminated string at DS:SI to the screen
    lodsb                         ; al = [ds:si], si++
    test    al,al
    jz      .done
    mov     ah,0x0e               ; BIOS teletype output
    mov     bx,0x0007             ; page 0, light-grey attribute
    int     0x10
    jmp     print
 .done:
    ret

;;;--------------------
;;; Boot splash (edit this string freely)
;;;--------------------
banner:
    db      13,10
    db      "10biForthOS",13,10
    db      "booted - speak Forth over serial (port 44444)",13,10,0

;;;--------------------
;;; Zero padding
;;;--------------------
%ifdef PAD_ZERO
    times 510-($-$$) db 0
    dw 0xAA55                   ; boot indicator
%endif
