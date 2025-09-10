# compiles both, fun :)

# Simply draws a 4D hypercube, I also translated it to python to send to my teacher as an assignment, because why not
# C version uses SDL2 for rendering and python version uses turtle for rendering (it's what the teacher wanted us to use for assignments)

gcc -o hypercube hypercube.c -lSDL2 -lm `sdl2-config --cflags`
