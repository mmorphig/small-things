import re
import subprocess

ascii_file = "ascii_output.txt"
video_file = "bad_apple.mp4" 
c_output = "ascii_data.h"

# Read ASCII data
with open(ascii_file, "r") as f:
    data = f.read()

# Extract FPS from the video using FFmpeg
fps = 30.0  # Default FPS in case of failure

try:
    result = subprocess.run(
        ["ffmpeg", "-i", video_file],
        stderr=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        text=True
    )
    fps_match = re.search(r'(\d+(\.\d+)?) fps', result.stderr)
    if fps_match:
        fps = float(fps_match.group(1))  # Convert to float for precision
except Exception as e:
    print(f"[ERROR] Could not extract FPS: {e}")

# Preserve empty frames properly by splitting at "\n\n"
frames = data.split("\n\n")  # Frames separated by double newlines
c_array = ',\n    '.join(f'"{frame.replace("\n", "\\n")}"' for frame in frames)

# Write to C header file
with open(c_output, "w") as f:
    f.write(f'#ifndef ASCII_DATA_H\n#define ASCII_DATA_H\n\n')
    f.write(f'const char *frames[] = {{\n    {c_array}\n}};\n')
    f.write(f'const int frame_count = {len(frames)};\n')
    f.write(f'const float ascii_fps = {fps};\n\n')
    f.write(f'#endif // ASCII_DATA_H\n')

print(f"Converted ASCII file to C header: {c_output} with FPS: {fps}")
