# 10biForth a 2 instructions Forth in 217 bytes for Linux Intel x64

[**10biForth**](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64) = **10b**(2) **i**(nstructions) **Forth** is a very primitive Forth with only two instructions:

- c is compile
- e is execute

It's a port of [**10biForthOS**](https://git.sr.ht/~hocwp/10biForthOS#10biforthos-a-full-8086-os-in-46-bytes) for Linux Intel x64.  
It is heavily inspired by [Frank Sergeant 3-Instruction Forth](https://pygmy.utoh.org/3ins4th.html) and is a strip down exercise following up [SectorForth](https://github.com/cesarblum/sectorforth), [SectorLisp](https://justine.lol/sectorlisp/), [SectorC](https://xorvoid.com/sectorc.html) and [milliForth](https://github.com/fuzzballcat/milliForth).

## Overview

When started, 10biForth listen the keyboard for instructions:  
The `c` instruction should be followed by a byte of an assembly opcode of two digits to be compiled into a fixed memory location.  
The `e` instruction execute the compiled program.

And that's it!  

## Usage

Compile and launch `10biForth`:

    $ ./cpl.sh 
    
    $ ls -l ./10biForth
    -rwxr-xr-x 1 217 ./10biForth
    
or once built once:

    $ ./10biForth

It now wait for keys to be entered. `10biForth` expect opcodes to be compiled.  
To find them, build what you want to compile into memory by writing a sample and translate it in instructions.

For example the simplest program can by an [exit syscall](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/examples/exit.asm) to leave the interpreter:

    $ ./transl examples/exit.asm 
    00000000  BF2A000000        mov edi,0x2a
    00000003  B83C000000        mov eax,0x3c
    00000008  0F05              syscall
    
    cBF c2A c00 c00 c00 cB8 c3C c00 c00 c00 c0F c05

Formatted like the disassembled code it looks like this:

    00000000  cBF c2A c00 c00 c00     mov edi,0x2a
    00000003  cB8 c3C c00 c00 c00     mov eax,0x3c
    00000008  c0F c05                 syscall

Enter (or paste) the sequence into the `10biForth` console and press `enter`.  
Press the `e` command and `enter`: the interpreter should exit.

     $ ./10biForth 
     cBF c2A c00 c00 c00 cB8 c3C c00 c00 c00 c0F c05
     e
     
     $ echo $?
     42

A more interesting program will be an [Hello world](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/examples/hello_world.asm) one:

    $ ./transl examples/hello_world.asm
    00000000  B801000000        mov eax,0x1
    00000005  BF01000000        mov edi,0x1
    0000000A  4C89F6            mov rsi,r14
    0000000D  4881C61F000000    add rsi,0x1f
    00000014  BA0E000000        mov edx,0xe
    00000019  0F05              syscall
    0000001B  4D89F5            mov r13,r14
    0000001E  C3                ret
    0000001F  48                rex.w
    00000020  656C              gs insb
    00000022  6C                insb
    00000023  6F                outsd
    00000024  2C20              sub al,0x20
    00000026  776F              ja 0x97
    00000028  726C              jc 0x96
    0000002A  64210A            and [fs:rdx],ecx
    
    cB8 c01 c00 c00 c00 cBF c01 c00 c00 c00 c4C c89 cF6 c48 c81 cC6 c1F c00 c00 c00 cBA c0E c00 c00 c00 c0F c05 c4D c89 cF5 cC3 c48 c65 c6C c6C c6F c2C c20 c77 c6F c72 c6C c64 c21 c0A 

Paste and execute (more than once):

    $ ./10biForth 
    cB8 c01 c00 c00 c00 cBF c01 c00 c00 c00 c4C c89 cF6 c48 c81 cC6 c1F c00 c00 c00 cBA c0E c00 c00 c00 c0F c05 c4D c89 cF5 cC3 c48 c65 c6C c6C c6F c2C c20 c77 c6F c72 c6C c64 c21 c0A 
    e
    Hello, world!
    
    e
    Hello, world!
    
    e
    Hello, world!

The interpreter is still listening and the code pointer [has been reset](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/examples/hello_world.asm#L12) to the start of the code memory at the end of `hello_world.asm`.  
We can write another program like [Hey!](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/examples/hey.asm)

    cB8 c01 c00 c00 c00 cBF c01 c00 c00 c00 c4C c89 cF6 c48 c81 cC6 c1F c00 c00 c00 cBA c09 c00 c00 c00 c0F c05 c4D c89 cF5 cC3 c48 c65 c79 c21 c21 c21 c21 c21 c0A 
    e
    Hey!!!!!
    e
    Hey!!!!!
    e
    Hey!!!!!
    e
    Hey!!!!!

The code does not need to be complete: [simple.asm](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/examples/simple.asm)

    $ ./transl examples/simple.asm
    00000000  B858000000        mov eax,0x58     <- a 'X' char 
    00000005  498907            mov [r15],rax
    00000008  B801000000        mov eax,0x1
    0000000D  BF01000000        mov edi,0x1
    00000012  4C89FE            mov rsi,r15
    00000015  BA01000000        mov edx,0x1
    0000001A  0F05              syscall
    0000001C  4D89F5            mov r13,r14
    0000001F  C3                ret
    
    cB8 c58 c00 c00 c00 c49 c89 c07 cB8 c01 c00 c00 c00 cBF c01 c00 c00 c00 c4C c89 cFE cBA c01 c00 c00 c00 c0F c05 c4D c89 cF5 cC3 
    e
    X
    e
    X

    cB8 c41     <- overwrite with a 'A'
    e
    A
    e
    A

And finally close the interpreter with the `exit` call above:

     c48 c31 cFF cB8 c3C c00 c00 c00 c0F c05 
     e
     
     $ echo $?
     0

An [execve example](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/examples/system.asm) allow to display the system information:

    $ echo -e "$(./transl examples/system.asm | tail -n 1) e\n" | tee /dev/fd/2 | ./10biForth
    cB8 c3B c00 c00 c00 c4C c89 cFF c48 c81 cC7 c24 c00 c00 c00 c48 c31 cD2 c52 c4C c89 cFE c48 c81 cC6 c3C c00 c00 c00 c56 c57 c48 c89 cE6 c0F c05 c2F c75 c73 c72 c2F c62 c69 c6E c2F c75 c6E c61 c6D c65 c00 c00 c00 c00 c00 c00 c00 c00 c00 c00 c2D c61 c00 c00 c00 c00 c00 c00 c00 c00 c00 c00 c00 c00 c00 c00  e

    Linux localhost 6.12.13-0-lts #1-Alpine SMP PREEMPT_DYNAMIC 2025-02-10 21:47:33 x86_64 Linux

## How it was built?

We want minimalism. We have to not care of conventions!  
Let's start with some minimal `elf64` versions. Three of them have been tried. Two of 120 bytes and one of 80 bytes.

Here are the starting point urls:

<https://research.h4x.cz/html/2019/2019-10-22--linux--minimal_viable_x86_elf64_static_binary.html>  
<https://ech0.re/building-the-smallest-elf-program/>  
<https://nathanotterness.com/2021/10/tiny_elf_modernized.html>

Unfortunately the [80 bytes one](https://ech0.re/building-the-smallest-elf-program/) failed to be extended. Maybe there is a solution with ELF header overlap (left as an exercise).

This [120 bytes one](https://research.h4x.cz/html/2019/2019-10-22--linux--minimal_viable_x86_elf64_static_binary.html) is a good start since the ELF header are already calculated.  
The hard work has already be done: many thanks `@echo 'cmVzLjdrM2F0QGg0eC5jego=' | base64 -d`!

The entry point are those 7 bytes which perform an exit syscall:

```asm
ELF 64 header code
...
; Code:
    ;db      0                 ;   e_ident[EI_ABIVERSION] (undef in Linux)
    ;times   7                 ;   e_ident[EI_PAD]        <--  [1]
_start:
    ;mov     edi,  edi          ; RDI = 0         -> arg1 for exit
    xor     eax,  eax          ; RAX = 0
    mov     al,   0x3c         ; RAX = 0x3c = 60 -> 'exit()' syscall
    mov     edi,  eax          ; RDI = RAX = 60  -> arg1 for exit
    syscall
; End of Code
...
```

Our program will not fit in those 7 bytes even if it is pretty small. Let replace it with a jump:

```asm
; Code:
_start:
    jmp     init               ; Not enough space in the header: jump to init
    times 6 db  0              ; padding to not crash the header
; End of Code
...
init:
    ....
```

Now that the program can start and we have enough space, we should have some more space to compile our interpreted code.  
The program should be minimal so we will allocate some anonymous memory with `mmap`.  
We want to execute our compiled code, so we will declare the memory with `PROT_READ | PROT_WRITE | PROT_EXEC` and `MAP_PRIVATE | MAP_ANONYMOUS` 
since we do not care of reading it from the disk.

```asm
init:
    ;;  Allocate some memory with mmap to store the code
    mov     r8, -1             ; anonymous memory - no file descriptor
    mov     rax, 9             ; mmap syscall number
    xor     rdi, rdi           ; operating system will choose mapping destination
    mov     rsi, MEM_SIZE      ; page size
    mov     rdx, MEM_PROT      ; new memory region will be marked rwe
    mov     r10, MEM_FLAGS     ; pages will not be shared

    xor     r9, r9             ; offset (unused)
    syscall                    ; now rax will point to mapped location
                               ; no error check: will segfault in this case
```

The rest of the code is nearly a port one to one of the keyboard version of [10biForhOS](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/8086-kbd/10biForthOS.asm)

The registers `r14` and `r15` have been reserved as follow:

```asm
;;; Register usage
;;;   r15    always code memory base pointer
;;;   r14    current code memory pointer
```

In this state our program takes 251 bytes. Let's try to save some:

The first obvious one is this `mov r8, -1      ; anonymous memory - no file descriptor` in the `mmap` setting.  
Let's try the standard trick with [`xor r8, r8` and `dec r8`](https://git.sr.ht/~hocwp/10biForthOS/commit/77766824c4186989ea8f8624835b6dced26bb6a7).  

```asm
    ...
init:
    ;;  Allocate some memory with mmap to store the code
    xor     r8,r8              ; anonymous memory - no file descriptor
    dec     r8                 ; equiv: mov r8, -1
    mov     rax, 9             ; mmap syscall number
    xor     rdi, rdi           ; operating system will choose mapping destination
    ...
```

Those two instructions are 6 bytes long. 1 byte shorter than the `mov r8, -1`. We have now **250 bytes**!

Hey, wait: 6 bytes. It's exactly the padding we have in the header after the `init` jump.  
Let's [use that space](https://git.sr.ht/~hocwp/10biForthOS/commit/34aaa6c74a71fe2870f1fcee88005043cefb06cf) with more header overlap:

```asm
...
; Code:
_start:
    xor     r8, r8             ; start of mmap syscall setting: anonymous memory
    dec     r8                 ; equiv: mov r8, -1
    jmp     init               ; Not enough space in the header: jump to init
; End of Code
...
```
The program compile and run nicely. We are now at **244 bytes**

Then, read [x86/include/asm/elf.h](https://elixir.bootlin.com/linux/v6.13.6/source/arch/x86/include/asm/elf.h#L156) and we see that all registers are granted to be zero at program startup.  
Let refactor and use this fact to do even more header overlap and register micro optimisation : [commit](https://git.sr.ht/~hocwp/10biForthOS/commit/34d682b1319568e2ee43a123a08162a71f00ef85)

The program still compile and run nicely. We are now at **227 bytes**.

Another trick to save 9 bytes is to use the return stack as a temporary buffer instead of the mmapped memory one: [commit](https://git.sr.ht/~hocwp/10biForthOS/commit/593ee215c4d3c0264523a97a1cadc4aeb3c359f9).

The program compile and run nicely. We are now at **218 bytes**

But wait once more. There is still an annoying `db 0` in the header padding. Let's refactor to use exactly 6 bytes in the header: [commit](https://git.sr.ht/~hocwp/10biForthOS/commit/c4022a5b3de8279a086dac929bb3f2a25de3b00e).

The program compile and run nicely. We are now at **217 bytes**

And the final(?) result is here: [10biForth.asm](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/10biForth.asm#L1)!

Just to compare, a standard ELF64 Linux **Hello world** takes **~4700 bytes**.  
This [10biForth.asm](https://git.sr.ht/~hocwp/10biForthOS/tree/master/item/x64/10biForth.asm#L1) takes more than 20 times 
less space with its **217 bytes**!!!

    $ sed examples/hello_world.asm \
        -e 's/64/64\nglobal _start\n_start:/' \
        -e 's/ret/xor rdi, rdi\n    mov rax, 60\n    syscall/' \
      > hello.asm

    $ nasm -f elf64 -o hello.o hello.asm
	
	$ ld -o hello hello.o
	
	$ ./hello
    Hello, world!
	
	$ ls -l ./hello
	... 4744 ...  ./hello
	
	$ ls -l ./10biForth
	...  217 ...  ./10biForth
	
The fun fact is that now the interpreter code takes 217 - 120 = 97 bytes: less than the ELF header size!

## Conclusion

One can say *« what's the point since we have terabytes of disk, ram, resources? »*

- First we do not have to waste all those resources. We can use some thinking or imagination to preserve them.

- Second this 217 bytes can be a starting point to build a more evolved Forth.

- Third this was fun :-]

And finally: « Is this a Forth? »

It seems to be: it has an outer interpreter which understand assembly opcodes and an inner interpreter: the standard machine code. A minimal switch/case dictionnary.  
You can load/redefine code at will.

Even if it lacks stacks, defining words, etc. It has the simplicity and hacky feeling of Forth.

**Happy hacking!**

*Disclaimer: this is an exercise in minimalism. A more evolved Forth should use a standard ELF format (or not ;-])*
