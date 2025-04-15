#!/bin/bash

# Converts videos to an executable that prints the video in a terminal as ascii
# You need to set the video file name in here and in helper.py

ffmpeg -i morning.mp4 -q:a 0 -map a audio.mp3 -y
python ./helperaudio.py

mkdir ./output
rm -f ./output/*
# get your own video :)
ffmpeg -i ./bad_apple.mp4 -vf "scale=1600:900" "output/frame_%04d.png"

# compile the image to text converter
mkdir ../build
gcc -c img2txt.c -o ../build/img2txt.o -O3
gcc ../build/img2txt.o -o img2txt.x86_64 -lpng 

./img2txt ./output 45 # number is the scale, default is 45
python ./helper.py

musl-gcc -o ascii_player.x86_64 ascii_player.c -O3 -ffunction-sections -fdata-sections -Wl,--gc-sections -static
strip --strip-all ascii_player.x86_64

# cleanup, but keep the created .h and .txt files
rm -f ./temp.bmp
rm -rf ./output

# Optional compression
#upx ./ascii_player
