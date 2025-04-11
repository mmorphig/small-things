#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Window dimensions
const int WIDTH = 2000;
const int HEIGHT = 1400;

// Grid dimensions
#define GRID_WIDTH 1000
#define GRID_HEIGHT 700

// Agent structure
typedef struct {
    float x, y;  // Position
    float dx, dy; // Direction
    int speciesIndex;
} Agent;

typedef struct {
		unsigned int color; // Colour
		int strength;
} Pheromone;

// Generic structure to hold variable name, pointer, and type
typedef struct {
    char* name;
    void* address;
    char type; // 'i' for int, 'f' for float, 'b' for bool
} ConfigVariable;

// A dynamic array for configuration variables
ConfigVariable* configVariables = NULL;
int configVariableCount = 0;

// Global variables
bool paused = false;
bool running = true;
bool debug = true;

// Configurable parameters, these are just the defaults though, priority is the config file at ./slimeconfig.txt
float decayRate = 0.99f;            // Rate at which pheromones decay
float diffusionRate = 0.0001f;      // Rate at which pheromones diffuse
float turnRate = 0.5f;        		  // Angle (radians) agents can turn per step
float sensorDistance = 10.0f;       // Distance to sample pheromones
float sensorRadius = 1.0f;          // Radius of the sensor
float lrSensorDistance = 3.0f;      // Increases Sensor distance of left-right sensors, multiplicative
float pheromoneStrength = 15.0f;    // Strength of pheromone deposition
float avoidanceStrength = 0.5f;     // Higher values increase avoidance intensity to other colours, slime racism
float pheromoneSensitivity = 2.0f;  // Sensitivity to pheromone differences, can get funky
float freeWill = 1.0f;              // Influences randomness in the turning of agents, works by multiplication
float speed = 1.0f;                 // Speed, multiplicative to base movement, may cause problems

// Total number of agents for each type
#define numSpecies 3 // Number of species, more hadcoded than other parameters
int speciesColor[] = {0x0000FF, 0xFF00FF, 0xFF0000};

int numAgents[] = {10000, 10000, 10000};
int totalAgents = 30000;

// Array of agents
Agent* agents = NULL;

// Pheromone grid
Pheromone pheromones[GRID_HEIGHT][GRID_WIDTH][numSpecies] = {0};

// Function prototypes
bool initializeSDL(SDL_Window** window, SDL_GLContext* glContext);
void handleInput(SDL_Event *event);
void updateSimulation();
void renderSimulation(SDL_Window* window);
void initializeAgents();
void depositPheromones(Agent* agent);
//GLuint pheromoneUpdateProgram;

// Initialize SDL and OpenGL context
bool initializeSDL(SDL_Window** window, SDL_GLContext* glContext) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow("Among Us", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
    if (*window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    *glContext = SDL_GL_CreateContext(*window);
    if (*glContext == NULL) {
        printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Set the swap interval for vsync
    SDL_GL_SetSwapInterval(0);

    // OpenGL settings
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background
    glPointSize(5.0f);
    
    return true;
}

char* readFile(const char* filePath) {
    FILE* file = fopen(filePath, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filePath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}

// Helper function to add a configuration variable dynamically
void addConfigVariable(const char* name, void* address, char type) {
    configVariables = realloc(configVariables, (configVariableCount + 1) * sizeof(ConfigVariable));
    configVariables[configVariableCount].name = strdup(name); // Duplicate the name
    configVariables[configVariableCount].address = address;
    configVariables[configVariableCount].type = type;
    configVariableCount++;
}

// Parses the config file and updates variables dynamically
void readConfig(const char* filePath) {
    char* configContent = readFile(filePath);
    if (!configContent) {
        return;
    }

    char* line = strtok(configContent, "\n");
    while (line) {
        char variable[64];
        char value[64];

        if (sscanf(line, "%63[^:]: %63s", variable, value) == 2) {
            int found = 0;
            for (int i = 0; i < configVariableCount; i++) {
                if (strcmp(configVariables[i].name, variable) == 0) {
                    found = 1;
                    if (configVariables[i].type == 'i') {
                        *((int*)configVariables[i].address) = atoi(value);
                    } else if (configVariables[i].type == 'f') {
                        *((float*)configVariables[i].address) = atof(value);
                    } else if (configVariables[i].type == 'b') {
                        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
                            *((bool*)configVariables[i].address) = true;
                        } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
                            *((bool*)configVariables[i].address) = false;
                        } else {
                            fprintf(stderr, "Invalid boolean value for %s: %s\n", variable, value);
                        }
                    }
                    break;
                }
            }
            if (!found) {
                fprintf(stderr, "Unknown variable: %s\n", variable);
            }
        }

        line = strtok(NULL, "\n");
    }

    free(configContent);
}

// Initialize Agents with Configurable Proportions
void initializeAgents() {
		agents = malloc(totalAgents * sizeof(Agent));
    if (!agents) {
        fprintf(stderr, "Failed to allocate memory for agents\n");
        exit(EXIT_FAILURE);
    }
	
    int agentIndex = 0;

		for (int j = 0; j < numSpecies; j++) {
		    for (int i = 0; i < numAgents[j]; i++) {
		        agents[agentIndex].x = rand() % GRID_WIDTH/10+(GRID_WIDTH-GRID_WIDTH/10)/2;
		        agents[agentIndex].y = rand() % GRID_WIDTH/10+(GRID_HEIGHT-GRID_WIDTH/10)/2;
		        float angle = ((float)rand() / RAND_MAX) * 2 * M_PI;
		        agents[agentIndex].dx = cos(angle);
		        agents[agentIndex].dy = sin(angle);
		        agents[agentIndex].speciesIndex = j; // Red
		        agentIndex++;
		    }
		}
}

void diffuseAndDecayPheromones() {
    #pragma omp parallel for
    for (int y = 1; y < GRID_HEIGHT - 1; y++) {
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int s = 0; s < numSpecies; s++) {
                // Cache pheromone values for efficiency
                Pheromone* center = &pheromones[y][x][s];
                Pheromone* north = &pheromones[y - 1][x][s];
                Pheromone* south = &pheromones[y + 1][x][s];
                Pheromone* west = &pheromones[y][x - 1][s];
                Pheromone* east = &pheromones[y][x + 1][s];

                // Diffuse pheromones
                float diffusedStrength = (
                    north->strength + south->strength +
                    west->strength + east->strength
                ) * diffusionRate;

                // Apply decay and self-influence directly
                center->strength = (center->strength + diffusedStrength) * decayRate;
                pheromones[y][x][s].strength *= decayRate;
            }
        }
    }
}

void updateSimulation() {
    diffuseAndDecayPheromones();

    #pragma omp parallel for num_threads(12)
    for (int i = 0; i < totalAgents; i++) {
        Agent* agent = &agents[i];

        // Precompute sensor positions
        float sensorDx = agent->dx * sensorDistance;
        float sensorDy = agent->dy * sensorDistance;
        float lrSensorDx = sensorDx * lrSensorDistance;
        float lrSensorDy = sensorDy * lrSensorDistance;

        // Clamped coordinates for each sensor position
        int forwardX = (int)(agent->x + sensorDx);
        int forwardY = (int)(agent->y + sensorDy);
        int leftX = (int)(agent->x + lrSensorDy);
        int leftY = (int)(agent->y - lrSensorDx);
        int rightX = (int)(agent->x - lrSensorDy);
        int rightY = (int)(agent->y + lrSensorDx);

        // Clamp to grid bounds
        forwardX = forwardX < 0 ? 0 : (forwardX >= GRID_WIDTH ? GRID_WIDTH - 1 : forwardX);
        forwardY = forwardY < 0 ? 0 : (forwardY >= GRID_HEIGHT ? GRID_HEIGHT - 1 : forwardY);
        leftX = leftX < 0 ? 0 : (leftX >= GRID_WIDTH ? GRID_WIDTH - 1 : leftX);
        leftY = leftY < 0 ? 0 : (leftY >= GRID_HEIGHT ? GRID_HEIGHT - 1 : leftY);
        rightX = rightX < 0 ? 0 : (rightX >= GRID_WIDTH ? GRID_WIDTH - 1 : rightX);
        rightY = rightY < 0 ? 0 : (rightY >= GRID_HEIGHT ? GRID_HEIGHT - 1 : rightY);

        // Sample pheromones for the agent's species
        float forwardStrength = pheromones[forwardY][forwardX][agent->speciesIndex].strength;
        float leftStrength = pheromones[leftY][leftX][agent->speciesIndex].strength;
        float rightStrength = pheromones[rightY][rightX][agent->speciesIndex].strength;

        // Apply penalties for other species' pheromones in a single loop
        for (int species = 0; species < numSpecies; species++) {
            if (species == agent->speciesIndex) continue;

            float tempForward = pheromones[forwardY][forwardX][species].strength;
            float tempLeft = pheromones[leftY][leftX][species].strength;
            float tempRight = pheromones[rightY][rightX][species].strength;

            forwardStrength -= avoidanceStrength * tempForward;
            leftStrength -= avoidanceStrength * tempLeft;
            rightStrength -= avoidanceStrength * tempRight;
        }

        // Ensure pheromone strengths are non-negative
        forwardStrength = fmaxf(forwardStrength, 0.0f);
        leftStrength = fmaxf(leftStrength, 0.0f);
        rightStrength = fmaxf(rightStrength, 0.0f);

        // Determine direction based on pheromone strength
        float maxStrength = fmaxf(fmaxf(forwardStrength, leftStrength), rightStrength);
        float angle = 0.0f;

        if (maxStrength > 0.0f) {
            if (forwardStrength >= leftStrength && forwardStrength >= rightStrength) {
                // Keep direction
            } else {
                float randomTurn = freeWill * ((float)rand() / RAND_MAX - 0.5f);
                if (leftStrength >= rightStrength) {
                    angle = turnRate * (leftStrength / maxStrength) * pheromoneSensitivity + randomTurn;
                } else {
                    angle = -turnRate * (rightStrength / maxStrength) * pheromoneSensitivity + randomTurn;
                }
            }
        }

        // Update direction
        float cosAngle = cosf(angle);
        float sinAngle = sinf(angle);
        float newDx = cosAngle * agent->dx - sinAngle * agent->dy;
        float newDy = sinAngle * agent->dx + cosAngle * agent->dy;
        agent->dx = newDx;
        agent->dy = newDy;

        // Move the agent
        agent->x += agent->dx * speed;
        agent->y += agent->dy * speed;

        // Handle boundary collisions
        if (agent->x < 0 || agent->x >= GRID_WIDTH) {
            agent->dx = -agent->dx;
            agent->x = fmaxf(0.0f, fminf(agent->x, GRID_WIDTH - 1));
        }
        if (agent->y < 0 || agent->y >= GRID_HEIGHT) {
            agent->dy = -agent->dy;
            agent->y = fmaxf(0.0f, fminf(agent->y, GRID_HEIGHT - 1));
        }

        // Deposit pheromones
		    pheromones[(int)agent->y][(int)agent->x][agent->speciesIndex].strength += pheromoneStrength;
    }
}

void renderSimulation(SDL_Window* window) {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);

    // Static buffers to avoid reallocation
    static float vertexBuffer[GRID_HEIGHT * GRID_WIDTH * 2]; // 2 floats per vertex (x, y)
    static float colorBuffer[GRID_HEIGHT * GRID_WIDTH * 3];  // 3 floats per vertex color (r, g, b)

    // Precompute constants
    static const float invGridWidth = 1.0f / (GRID_WIDTH - 1);
    static const float invGridHeight = 1.0f / (GRID_HEIGHT - 1);

    // Initialize vertex buffer
    #pragma omp parallel for
    for (int i = 0; i < GRID_HEIGHT * GRID_WIDTH; i++) {
        int x = i % GRID_WIDTH;
        int y = i / GRID_WIDTH;

        // Compute normalized device coordinates
        float ndcX = x * invGridWidth * 2.0f - 1.0f;
        float ndcY = 1.0f - y * invGridHeight * 2.0f;

        // Store vertex position
        vertexBuffer[i * 2] = ndcX;
        vertexBuffer[i * 2 + 1] = ndcY;
    }

    // Update color buffer
    #pragma omp parallel for
    for (int i = 0; i < GRID_HEIGHT * GRID_WIDTH; i++) {
        int x = i % GRID_WIDTH;
        int y = i / GRID_WIDTH;

        float r = 0.0f, g = 0.0f, b = 0.0f;

        // Aggregate pheromone strengths for all species
        for (int s = 0; s < numSpecies; s++) {
            float strength = pheromones[y][x][s].strength / 255.0f;

            // Extract color components from speciesColor
            float sr = ((speciesColor[s] >> 16) & 0xFF) / 255.0f;
            float sg = ((speciesColor[s] >> 8) & 0xFF) / 255.0f;
            float sb = (speciesColor[s] & 0xFF) / 255.0f;

            // Blend colors based on pheromone strength
            r += sr * strength;
            g += sg * strength;
            b += sb * strength;
        }

        // Clamp colors to [0, 1]
        colorBuffer[i * 3] = fminf(r, 1.0f);
        colorBuffer[i * 3 + 1] = fminf(g, 1.0f);
        colorBuffer[i * 3 + 2] = fminf(b, 1.0f);
    }

    // Enable vertex and color arrays
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // Set array pointers
    glVertexPointer(2, GL_FLOAT, 0, vertexBuffer);
    glColorPointer(3, GL_FLOAT, 0, colorBuffer);

    // Draw points
    glDrawArrays(GL_POINTS, 0, GRID_WIDTH * GRID_HEIGHT);

    // Disable arrays
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    // Swap buffers to display the frame
    SDL_GL_SwapWindow(window);
}

// Handle user input
void handleInput(SDL_Event *event) {
    switch (event->type) {
        case SDL_KEYDOWN:
            switch (event->key.keysym.sym) {
                case SDLK_p:
                    paused = !paused;
                    break;
                case SDLK_RETURN:
                    if (debug) {
                        running = true;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
}

int main(int argc, char *argv[]) {
		// Dynamically register configuration variables
    addConfigVariable("numRedAgents", &numAgents[0], 'i');
    addConfigVariable("numGreenAgents", &numAgents[1], 'i');
    addConfigVariable("numBlueAgents", &numAgents[2], 'i');
    addConfigVariable("decayRate", &decayRate, 'f');
    addConfigVariable("diffusionRate", &diffusionRate, 'f');
    addConfigVariable("turnRate", &turnRate, 'f');
    addConfigVariable("sensorDistance", &sensorDistance, 'f');
    addConfigVariable("sensorRadius", &sensorRadius, 'f');
    addConfigVariable("lrSensorDistance", &lrSensorDistance, 'f');
    addConfigVariable("pheromoneStrength", &pheromoneStrength, 'f');
    addConfigVariable("avoidanceStrength", &avoidanceStrength, 'f');
    addConfigVariable("pheromoneSensitivity", &pheromoneSensitivity, 'f');
    addConfigVariable("freeWill", &freeWill, 'f');
    addConfigVariable("speed", &speed, 'f');

    // Read and apply configuration from file
    const char* configPath = "slimeconfig.txt";
    readConfig(configPath);

    // Output the loaded configuration for verification
    printf("numRedAgents: %d\n", numAgents[0]);
    printf("numGreenAgents: %d\n", numAgents[1]);
    printf("numBlueAgents: %d\n", numAgents[2]);
    printf("decayRate: %f\n", decayRate);
    printf("diffusionRate: %f\n", diffusionRate);
    printf("turnRate: %f\n", turnRate);
    printf("sensorDistance: %f\n", sensorDistance);
    printf("sensorRadius: %f\n", sensorRadius);
    printf("lrSensorDistance: %f\n", lrSensorDistance);
    printf("pheromoneStrength: %f\n", pheromoneStrength);
    printf("avoidanceStrength: %f\n", avoidanceStrength);
    printf("pheromoneSensitivity: %f\n", pheromoneSensitivity);
    printf("freeWill: %f\n", freeWill);
    printf("speed: %f\n", speed);
    printf("\n"); // Padding
    
    totalAgents = numAgents[0] + numAgents[1] + numAgents[2];

    SDL_Window* window = NULL;
    SDL_GLContext glContext;
    SDL_Event event;

    if (!initializeSDL(&window, &glContext)) {
        printf("Failed to initialize SDL!\n");
        return -1;
    }
    
    //printf("%02X\n", window);
    /*
    pheromoneUpdateProgram = loadComputeShaderProgram("pheromone_update.glsl");
    if (pheromoneUpdateProgram = 0) {
        printf("Error loading compute shader program\n");
    }*/

    initializeAgents();

    // Variables for FPS calculation
    Uint32 currentTime = SDL_GetTicks();
    Uint32 fpsStartTime = SDL_GetTicks(); // Time at the start of the current FPS calculation period
    int frameCount = 0;                   // Count of frames rendered during the current period
	int minFrameCount;
	int avgFrameCount = 0;
	int maxFrameCount = 0;
	Uint32 timeStartTime = SDL_GetTicks();
    Uint32 updateSimulationTime;
    Uint32 renderSimulationTime;
    while (running) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            handleInput(&event);
        }

        if (!paused) {
						Uint32 currentTime = SDL_GetTicks();
            updateSimulation();
            updateSimulationTime = SDL_GetTicks() - currentTime;
        }
				
				currentTime = SDL_GetTicks();
		if (!paused) {
			renderSimulation(window);
		}
        renderSimulationTime = SDL_GetTicks() - currentTime;

        // Increment frame counter
        frameCount++;
		
		if (!paused) {
			// Cycle RGB colors smoothly
			float r = (sin(currentTime * 0.0005f + 2.0f * M_PI / 3.0f) + 1.0f) / 2.0f; // Sine wave between 0 and 1
			float g = (sin(currentTime * 0.0005f + 2.0f * M_PI / 3.0f) + 1.0f) / 2.0f;
			float b = (sin(currentTime * 0.0005f + 4.0f * M_PI / 3.0f) + 1.0f) / 2.0f;
		
			// Convert to 24-bit color
			speciesColor[0] = ((int)(r * 255) << 16) | ((int)(g * 128) << 8) | (int)(b * 255);
		}
		
        // Calculate FPS every second
        currentTime = SDL_GetTicks();
        if (currentTime - fpsStartTime >= 1000) { // 1 second elapsed
            printf("FPS: %dHz\tmspf: %.2fms\n", frameCount, (float)1000/(float)frameCount); // Output FPS to stdout
            printf("ms updateSimulation: %ims\n", updateSimulationTime);
            printf("ms renderSimulation: %ims\n", renderSimulationTime);
            if (frameCount < minFrameCount) {
								minFrameCount = frameCount;
						}
						if (frameCount > maxFrameCount) {
								maxFrameCount = frameCount;
						}
						avgFrameCount = (frameCount + avgFrameCount)/2; 
            frameCount = 0;                 // Reset frame counter
            fpsStartTime = currentTime;     // Reset start time for the next period
        }
        SDL_Delay(16);
    }
   
    printf("\nMinimum FPS: %dHz\tmspf: %.2fms\n", minFrameCount, (float)1000/(float)minFrameCount);
    printf("Average FPS: %dHz\tmspf: %.2fms\n", avgFrameCount, (float)1000/(float)avgFrameCount);
    printf("Maximum FPS: %dHz\tmspf: %.2fms\n", maxFrameCount, (float)1000/(float)maxFrameCount);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // Free dynamically allocated memory (if applicable)
    free(agents);
    
    for (int i = 0; i < configVariableCount; i++) {
        free(configVariables[i].name);
    }
    free(configVariables);

    return 0;
}
