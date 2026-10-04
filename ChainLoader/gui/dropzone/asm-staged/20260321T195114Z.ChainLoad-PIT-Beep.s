# compile: gcc -o beep.o -c beep.s ; ld -o beep beep.o
# run: sudo ./beep, non-root userspace code is not allowed to use port IO

    .global _start
    .text

_start:
    # ioperm(0x42, 32, 1)
    mov     $173, %rax  # ioperm() == 173 in x86_64
    mov     $0x42, %rdi # from port 0x42
    mov     $32, %rsi   # num ports, to port 0x62
    mov     $1, %rdx    # enable
    syscall
    cmpl    $0, %eax
    je      2f

    # ioperm() returned error,
    # write(1, message, 13)
    mov     $1, %rax    # write() == system call 1
    mov     $1, %rdi    # stdout
    mov     $message, %rsi
    mov     $37, %rdx   # message length
    syscall
    mov     $1, %rdi    # set retcode 1
    jmp     _exit

    # send "set tune" command
2:  movb    $0xB6, %al
    outb    %al, $0x43

    # nanosleep to let the IO complete                                                                                                                                  [3/62]
    movl    $0x1000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b

    # set 220Hz, 0x152F == 1193180 / 220
    movb    $0x2F, %al
    outb    %al, $0x42

    # nanosleep
    movl    $0x1000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b

    movb    $0x15, %al
    outb    %al, $0x42

    # nanosleep
    movl    $0x1000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b

    # turn on the speaker
    inb     $0x61, %al
    movb    %al, %ah
    orb     $0x3, %al
    outb    %al, $0x61

    # sleep about 1 sec
    movl    $0x30000000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b

    # turn off thespeaker
    movb    %ah, %al
    andb    $0xfc, %al
    outb    %al, $0x61

    xor     %rdi, %rdi  # set return code 0
_exit:      # exit(%rdi)
    mov     $60, %rax   # exit() == system call 60
    syscall

message:
    .ascii  "ioperm() failed, run as root or sudo\n"