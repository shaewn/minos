#!/bin/bash
special="-s -S"
if [[ $1 == "ndbg" ]]; then
    special=
fi
qemu-system-aarch64 $special -m 1024M -smp 10 -M virt -cpu max -nographic -kernel kernel.elf
