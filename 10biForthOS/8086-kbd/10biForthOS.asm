    bits 16
    cpu 8086

    [org 0x7c00]

EXECUTE equ 0x7e00

;;;--------------------
;;; Init
;;;--------------------
init:
    mov     ax,EXECUTE
    mov     ds,ax
    xor     di,di

    mov     al,'$'
    call    __putchar

;;;--------------------
;;; Interpreter loop
;;;--------------------
loop:
    call    __getchar

    cmp     al,'0'
    jne     .skip
    jmp     EXECUTE:0x0
 .skip:

    cmp     al,'1'
    je      compile

    mov     al,'?'
    call    __putchar

    jmp     loop

;;;--------------------
;;; Compile
;;;--------------------
compile:
    call    __getdigit
    mov     cl,4
    sal     al,cl
    mov     dl,al

    call    __getdigit
    or      al,dl

    mov     byte [di],al
    inc     di

    jmp     loop

;;;--------------------
;;; Utils
;;;--------------------
__getchar:                      ; get char in al
    xor     ax,ax
    int     0x16
    mov     bp,ax
__putchar:                      ; put char in al
    mov     ah,0x0E
    mov     bh,0x00
    int     0x10
    cmp     al,13
    jne     .RET
    mov     al,10
    jmp     __putchar
.RET:
    xchg     ax,bp
    ret

__getdigit:                     ; get a digit in al
    call    __getchar
    cmp     al,'A'
    jl      .DIGIT
    add     al,9
 .DIGIT:
    and     al,0x0F
    ret

;;;--------------------
;;; Zero padding
;;;--------------------
%ifdef PAD_ZERO
    times 510-($-$$) db 0
    dw 0xAA55                   ; boot indicator
%endif
