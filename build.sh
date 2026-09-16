#!/bin/bash

g++ -g -O3 main.cpp -o main -lraylib
python asm.py bios.asm bios.bin
python asm.py program.asm disk.img
truncate -s 1440K disk.img
