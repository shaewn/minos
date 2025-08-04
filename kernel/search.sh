#!/bin/bash

a=$0
echo ${a:(-9):9}

if [[ ${0:(-6):6} == "elfsym" ]]; then
    echo Yes!
elif [[ ${0:(-4):4} == "osym" ]]; then
    echo Yes 2!
fi
