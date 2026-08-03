#!/bin/sh
gcc -O3 -ffast-math -march=native -mtune=native -flto -fomit-frame-pointer -fno-math-errno -o vqsdft vqsdft.c -lm
strip vqsdft
