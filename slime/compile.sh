# Slime mould simulation, does not run super well and I should probably switch it to pthread and not openmp
# Yes, it's running on the CPU

mkdir ../build

gcc -c slime.c -o ../biuld/slime.o -O3 -march=native -ffast-math -fopenmp
gcc ../biuld/slime.o -o slime.x86_64 -lSDL2 -lm -lGLEW -lGL `sdl2-config --libs` -fopenmp

# static version
gcc -c slime.c -o ../biuld/slimes.o -O3 -ffast-math -fopenmp
gcc ../biuld/slimes.o -o slimes.x86_64 -static -lSDL2 -lm -lGLEW -lGL `sdl2-config --libs` -fopenmp
