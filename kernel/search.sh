#!/bin/bash

a=$0

if [[ ${0:(-6):6} == "elfsym" ]]; then
    full_text=$(aarch64-linux-gnu-objdump -t kernel.elf)
elif [[ ${0:(-4):4} == "osym" ]]; then
    full_text=$(find -name "*.o" -exec aarch64-linux-gnu-objdump -t {} +)
fi

if (( $# == 0 )); then
    echo "You must supply a regular expression to search for"
    exit
fi

echo "$full_text" | rg ${*:1:$#}
