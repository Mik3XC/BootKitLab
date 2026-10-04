    bits 64

    mov   rax, 59               ; execve

    mov   rdi, r15              ; uname: relative to r15
    add   rdi, command          ; in the allocated memory

    ;; arguments array construction
    xor   rdx, rdx              ; NULL

    push  rdx                   ; End of array

    mov   rsi, r15              ; "-a"
    add   rsi, argument1
    push  rsi

    push  rdi                   ; "/usr/sbin.uname"

    mov   rsi, rsp              ; rsi = argument array pointer

    syscall                     ; execve will exit

command:    dq "/usr/bin/uname", 0x0
argument1:  dq "-a", 0x0
