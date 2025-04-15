#!/bin/bash

# Check if ASCII file exists
ASCII_FILE="ascii_output.txt"

if [ ! -f "$ASCII_FILE" ]; then
    echo "Error: $ASCII_FILE not found!"
    exit 1
fi

# Read the file and split frames by double newlines
IFS='' # Preserve whitespace
frames=()
current_frame=""

while IFS= read -r line || [ -n "$line" ]; do
    if [[ -z "$line" ]]; then
        frames+=("$current_frame")
        current_frame=""
    else
        current_frame+="$line"$'\n'
    fi
done < "$ASCII_FILE"

# Loop through frames at 30 FPS
while true; do
    for frame in "${frames[@]}"; do
        clear
        echo "$frame"
        sleep 0.033 # 30 FPS (1/30 seconds)
    done
done
