mp3_file = "audio.mp3" 
c_output = "mp3_data.h"

# Read the MP3 file as binary data
with open(mp3_file, "rb") as f:
    data = f.read()

# Convert MP3 bytes into a C array
hex_data = ', '.join(f'0x{b:02x}' for b in data)

# Write to C header file
with open(c_output, "w") as f:
    f.write(f'#ifndef MP3_DATA_H\n#define MP3_DATA_H\n\n')
    f.write(f'static const unsigned char mp3_data[] = {{\n    {hex_data}\n}};\n')
    f.write(f'static const unsigned int mp3_size = {len(data)};\n\n')
    f.write(f'#endif // MP3_DATA_H\n')

print(f"Converted {mp3_file} to C header: {c_output}")
