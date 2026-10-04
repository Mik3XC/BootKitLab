#! /bin/sh

nasm 10biForth.asm
chmod a+x 10biForth

ls -l ./10biForth

./10biForth; echo $?
