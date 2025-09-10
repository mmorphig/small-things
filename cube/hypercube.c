#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define NUM_POINTS 16
#define ROTATE_SPEED 0.01f

typedef struct {
    float x, y, z, w;
} Point4D;

typedef struct {
    int a, b;
} Edge;

// Vertices of a 4D hypercube
Point4D points[NUM_POINTS] = {
    {-1, -1, -1, -1}, {-1, -1, -1, 1}, {-1, -1, 1, -1}, {-1, -1, 1, 1},
    {-1, 1, -1, -1},  {-1, 1, -1, 1},  {-1, 1, 1, -1},  {-1, 1, 1, 1},
    {1, -1, -1, -1},  {1, -1, -1, 1},  {1, -1, 1, -1},  {1, -1, 1, 1},
    {1, 1, -1, -1},   {1, 1, -1, 1},   {1, 1, 1, -1},   {1, 1, 1, 1}
};

// Edges connecting the vertices
Edge edges[] = {
    {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3}, {2, 6}, {3, 7},
    {4, 5}, {4, 6}, {5, 7}, {6, 7}, {8, 9}, {8, 10}, {8, 12}, {9, 11},
    {9, 13}, {10, 11}, {10, 14}, {11, 15}, {12, 13}, {12, 14}, {13, 15}, {14, 15},
    {0, 8}, {1, 9}, {2, 10}, {3, 11}, {4, 12}, {5, 13}, {6, 14}, {7, 15}
};

void rotate4D(Point4D *p, float angleXW, float angleYW, float angleZW) {
    float sinXW = sinf(angleXW), cosXW = cosf(angleXW);
    float sinYW = sinf(angleYW), cosYW = cosf(angleYW);
    float sinZW = sinf(angleZW), cosZW = cosf(angleZW);

    float newX = p->x * cosXW - p->w * sinXW;
    float newW = p->x * sinXW + p->w * cosXW;
    p->x = newX;
    p->w = newW;

    float newY = p->y * cosYW - p->w * sinYW;
    newW = p->y * sinYW + p->w * cosYW;
    p->y = newY;
    p->w = newW;

    float newZ = p->z * cosZW - p->w * sinZW;
    newW = p->z * sinZW + p->w * cosZW;
    p->z = newZ;
    p->w = newW;
}

void projectAndDraw(SDL_Renderer *renderer, Point4D *points, int num_points, Edge *edges, int num_edges) {
    for (int i = 0; i < num_edges; i++) {
        Point4D p1 = points[edges[i].a];
        Point4D p2 = points[edges[i].b];

        // Project from 4D to 3D
        float distance = 3;
        p1.x /= (distance - p1.w);
        p1.y /= (distance - p1.w);
        p1.z /= (distance - p1.w);

        p2.x /= (distance - p2.w);
        p2.y /= (distance - p2.w);
        p2.z /= (distance - p2.w);

        // Project from 3D to 2D
        int screenX1 = WINDOW_WIDTH / 2 + (int)(p1.x * 200);
        int screenY1 = WINDOW_HEIGHT / 2 + (int)(p1.y * 200);
        int screenX2 = WINDOW_WIDTH / 2 + (int)(p2.x * 200);
        int screenY2 = WINDOW_HEIGHT / 2 + (int)(p2.y * 200);

        SDL_RenderDrawLine(renderer, screenX1, screenY1, screenX2, screenY2);
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Rotating 4D Hypercube", 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    float angleXW = 0, angleYW = 0, angleZW = 0;

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        angleXW += ROTATE_SPEED;
        angleYW += ROTATE_SPEED;
        angleZW += ROTATE_SPEED;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        Point4D rotatedPoints[NUM_POINTS];
        for (int i = 0; i < NUM_POINTS; i++) {
            rotatedPoints[i] = points[i];
            rotate4D(&rotatedPoints[i], angleXW, angleYW, angleZW);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        projectAndDraw(renderer, rotatedPoints, NUM_POINTS, edges, sizeof(edges) / sizeof(edges[0]));

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}