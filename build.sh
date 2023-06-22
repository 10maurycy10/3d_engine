#!/bin/sh
gcc map.c main.c -o game -Wall -std=c99 -lSDL2 -lm -gdwarf -Ofast
