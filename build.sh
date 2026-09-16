#!/bin/bash

g++ -g -O3 main.cpp -o main -lraylib
python asm.py test.asm test.bin
