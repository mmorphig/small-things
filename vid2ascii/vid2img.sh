#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <input.mp4> <output_folder>"
    exit 1
fi

input="$1"
output="$2"

mkdir -p "$output"

ffmpeg -i "$input" -vf "scale=160:90" "$output/frame_%04d.png"

echo "Images saved in $output/"
