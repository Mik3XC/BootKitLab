all: run
.PHONY: dis odump floppy 10biForthOS.com

10biForthOS.com: 10biForthOS.asm
	nasm -f bin 10biForthOS.asm -o 10biForthOS.com
	wc -c 10biForthOS.com

10biForthOS.flp: 10biForthOS.asm 10biForthOS.com
	nasm -DPAD_ZERO -f bin 10biForthOS.asm -o 10biForthOS.com
	dd status=noxfer conv=notrunc if=10biForthOS.com of=10biForthOS.flp

run: 10biForthOS.flp
	./boot

dis: clean 10biForthOS.com
	ndisasm -b 16 10biForthOS.com

odump: clean 10biForthOS.com
	objdump -D -Mintel,i8086 -b binary -m i386 10biForthOS.com

floppy: 10biForthOS.flp
	truncate -s 320k 10biForthOS.flp
	@echo "===> 10biForthOS.flp floppy size is: c=40 h=2 s=8"

clean:
	rm -f *.com *.flp *.o
