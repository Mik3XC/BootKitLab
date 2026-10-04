;;; Example taken from https://github.com/pbrochard/subleq-eForthOS
;;; Unlicense license <https://unlicense.org>

    xor ah, ah                  ; set video mode
    mov al, 0x02
    int 0x10

    call cls

    xor dx, dx
    call moveto

    mov ax, 'G'
    call putchar
    mov ax, 'o'
    call putchar
    mov ax, 13
    call putchar

    call getchar
    call putchar

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; SUBLEQ                                         ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; 0. if (pc < 0) halt
;;; pc += 3
;;; 1. A == -1  [B] = getchar
;;; 2. B == -1  putchar [A]
;;; 3. [B] = [B] - [A]; if ([B] <= 0) pc = C

    xor si, si                          ; pc = SI

subleq:
    cmp si, 0
    jl  exit

    mov ax, [data + si]                 ; A = AX
    mov bx, [data + si + 2]             ; B = BX
    mov cx, [data + si + 4]             ; C = CX

    add si, 6

    cmp ax, -1                          ; 1. A == -1 ?  [B] = getchar
    je  .A_NEG_GETCHAR

    cmp bx, -1                          ; 2. B == -1 ?  putchar [A]
    je  .B_NEG_PUTCHAR

    jmp .ELSE                           ; 3. [B] = [B] - [A]; if ([B] <= 0) pc = C

 .A_NEG_GETCHAR:
    mov di, bx
    shl di, 1
    call getchar
    mov [data + di], ax
    jmp subleq

 .B_NEG_PUTCHAR:
    mov di, ax
    shl di, 1
    mov ax, [data + di]
    call putchar
    jmp subleq

 .ELSE:
    mov di, ax                          ; 3. [B] = [B] - [A]
    shl di, 1
    mov ax, [data + di]

    mov di, bx
    shl di, 1
    mov bx, [data + di]

    sub bx, ax

    mov [data + di], bx

    cmp bx, 0
    jg subleq                           ; [B] > 0

    shl cx, 1                           ; [B] <= 0 ! pc = c
    mov si, cx
    jmp subleq


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;          Subroutines                           ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
exit:
    mov ax, 'X'
    call putchar
    jmp 0:0x7c00

cls:
    mov ah, 0x07                ; tells BIOS to scroll down window
    mov al, 0x00                ; clear entire window
    mov bh, 0x07                ; white on black
    mov cx, 0x00                ; specifies top left of screen as (0,0)
    mov dh, 0x18                ; 18h = 24 rows of chars
    mov dl, 0x4f                ; 4fh = 79 cols of chars
    int 0x10                    ; calls video interrupt
    ret

moveto:                         ; move cursor to DH = Row, DL = Column
    mov ah, 0x02
    mov bh, 0x00
    int 0x10
    ret

putchar:                        ; put char in al
    mov ah, 0x0E
    mov bh, 0x00
    int 0x10
    cmp al, 13
    je  .LF
    ret
 .LF:
    mov al, 10
    jmp putchar

getchar:                        ; get char in al
    mov ah, 0x00
    int 0x16
    xor ah, ah

    push ax                     ; echo char
    push bx
    call putchar
    pop bx
    pop ax
    ret

data:
    %include "./examples/subleq-eForth/eforth-dec.asm"
