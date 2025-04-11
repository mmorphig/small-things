# Reads an input file and outputs the hex editor-like output to the stdout (printf), can be easliy put into a file with bash
# I only made this tiny little thing because I could not find a package that had exactly what I wanted

mkdir ../build

gcc -c hexstdout.c -o ../build/hexstdout.o
gcc ../build/hexstdout.o -o hexstdout.x86_64 -lm
