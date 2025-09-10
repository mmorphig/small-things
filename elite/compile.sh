# 3D space renderer thing, uses only the CPU for graphics and all, uses OpenGL and SDL2.
# Very inefficient, I get 50-140 fps (depending what is being rendered) on my Ryzen 5 7640U.
# There are some screenshots as well, there is a built-in screenshot functionality.
# Also a lot of redundant functions for mathematical functions.
# Included in this directory is tobin.c, it can convert .stl files to .bin files the engine can read 
# Not very good, after all, it's a learning project for me, a LOT of vector math and quaternion rotation that I have not done before.
# Really just 2000 lines of going insane
# Some compile flags might be useless, I don't care enough to change them

# wasd for the player
# qe for roll, rf for pitch
# arrow keys for the camera rotation
# ijkl for the camera movement
# uo for camera roll
# lshift move camera up
# lctrl move camera down
# z first person
# x free look
# p pause
# 0 take screenshot

mkdir build

# Compile pause_menu.c
gcc -c pause_menu.c -o build/pmenu.o -Wall -Wextra -msse4.1 -O3 -ffast-math -funroll-loops -fomit-frame-pointer -mavx `sdl2-config --cflags` -fopenmp

# Compile elite.c
gcc -g -c elite.c -o build/elite.o `sdl2-config --cflags` -msse4.1 -fopenmp -O3 -ffast-math -funroll-loops -fomit-frame-pointer

# Create the executable
gcc -g build/pmenu.o build/elite.o -o elite.x86_64 -lSDL2 -lm -lGLEW -lGL `sdl2-config --libs` -fopenmp -flto -lGLU
