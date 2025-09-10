#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <pthread.h>
#include "pause_menu.h"

#define COLLISION_DISTANCE 5.0f // Max distance to test collison
#define TURN_SPEED 0.01f
#define LINE_THRESHOLD 2.0f  // Pixel distance to mark an edge
#define STAR_SEED 31415926
#define SKYBOXSTAR_COUNT 2500
#define SKYBOXSTAR_MAX_DISTANCE 1600000000
#define SKYBOXSTAR_MIN_DISTANCE 50000
#define DRAG_STRENGTH 1.0f
#define MOVEMENT_DAMPENING 0.04f
#define MAX_VECTOR_LENGTH 5.0f  // Limit how far the object tries to move per frame
#define MIN_VECTOR_LENGTH 0.1f  // Prevents getting stuck due to extremely small movement
#define CLAMP(t, min, max) fmaxf(min, fminf(max, t))

#define NUM_THREADS 1

typedef struct {
    float position[3];
    unsigned int color;
    float size;
} SkyboxStar; // Different from star, these are in the background

typedef struct {
	float position[3];
    unsigned int color;
    float size;
    float brightness;
    float spin;
} Star;

typedef struct {
	float position[3];
    unsigned int color;
    float size;
    float atmosphereStrength;
    float spin;
} Planet;

typedef struct {
    float w, x, y, z;
} Quaternion;

typedef struct {
    float x, y, z;
} Vec3;

typedef enum {
    TYPE_PLANET,
    TYPE_OBJECT
} RenderType;

typedef struct {
    RenderType type;
    float distance;
    int index;
} DrawableDistance;

typedef struct {
    float v1[3];
    float v2[3];
    float v3[3];
} Triangle;

// Pathfining parameters or whatever
typedef struct {
	float position[3]; // Desired location
	float velX, velY, velZ; // Desired speed
	float strength; // How much it wants to go there
} PathDestination;

typedef struct {
	PathDestination* destinations; // Can have many distinations, of varying strength
	uint32_t numDestinations;
	int chasing; // Practically a bool, if it's current action is a chase
} PathParameters;

typedef struct {
	uint64_t objectIndex;
	float drag;
	float rollSpeed, yawSpeed, pitchSpeed;
	float forwardSpeed, backwardSpeed;
	float minChaseDistance, maxChaseDistance;
	float health;
} ShipParameters;

typedef struct {
	uint8_t personality; // 8-bit 'number' use the bits for individual aspects?
} MobParameters;

typedef struct {
	uint8_t id; // todo: assign ids ids are the type of ship
	Triangle* triangles;
	Vec3 triangle_normals;
	float position[3]; // Centre of object based on where the verticies are
	size_t triangle_count;
	unsigned int color;
	float velX, velY, velZ;
    float forward[3];
    float up[3];
    float right[3];
    PathParameters pathing;
    ShipParameters parameters;
    MobParameters mob;
    int invincible, invisible;
    float avoidanceRadius;
    float mass;
    uint32_t planetIndex, starIndex;
} Object;

typedef struct {
    Object* objects;
    int start;
    int end;
} ThreadData;

// Function prototypes, put here when needed lol
float getDistance3D(Vec3 a, Vec3 b);
void calculateObjectCenter(const Object* object, float center[3]);
void fnormalize(float v[3]);
void computeYawPitch(float forward[3], float target[3], float* outYaw, float* outPitch);
void rotateX(float point[3], float angle);
void rotateY(float point[3], float angle);
void rotateZ(float point[3], float angle);
void rotateObject(Object* object, float pitch, float yaw, float roll);
void rotateObjectAroundAxis(Object* object, float axis[3], float angle);
void fnormalize(float v[3]);
Quaternion axisAngleToQuaternion(float axis[3], float angle);
Quaternion rotationMatrixToQuaternion(float forward[3], float up[3], float right[3]);
Quaternion rotationBetweenVectors(float v1[3], float v2[3]);
void rotatePointByQuaternion(float point[3], Quaternion q);
void rotateObjectByQuaternion(Object* object, Quaternion q);
Quaternion slerp(Quaternion q1, Quaternion q2, float t);
void turnTowardsPoint(Object* object, float target[3]);
Quaternion multiplyQuat(Quaternion q1, Quaternion q2);
Quaternion axisAngleToQuat(Vec3 axis, float angle);
void rotateCamera(Vec3 axis, float angle);
Vec3 rotateVecByQuat(Vec3 v, Quaternion q);
Vec3 crossProduct(Vec3 a, Vec3 b);
Vec3 normalize(Vec3 v);
Quaternion normalizeQuaternion(Quaternion q);
void moveObject(Object* object, float moveX, float moveY, float moveZ);

SkyboxStar SkyboxStars[SKYBOXSTAR_COUNT];

Object* objects = NULL;
int numObjects = 0;
int* availableObjectIndexes = NULL; // Only has a value once an object has been cleared out, not when the object list can be expanded

Planet* planets = NULL;
int numPlanets = 0;

Star* stars = NULL;
int numStars = 0;

const double G = 6.67430e-11; // Gravitational constant

// Camera parameters.
float cameraSpeed = 10.0f;
Vec3 cameraPos = {20000.0f, 0.0f, -500.0f};
Vec3 camForward;
Vec3 camRight;
Vec3 camUp;
Quaternion cameraOrientation = {0, 0, 1, 0}; // Identity quaternion
int freeLook; // Is able to look around while camera locked?

Uint32 firstPersonTime, freeLookTime, pauseTime;

// Focal length in pixels (for a 90° FOV).
float f = SCREEN_WIDTH / 2.0f;

int running = 1;
int paused = 0;
int firstPerson = 0;

const float LINE_THRESHOLD_SQR = LINE_THRESHOLD * LINE_THRESHOLD;

const float CLAMPED_THRESHOLD = LINE_THRESHOLD - 0.5f;
const float CLAMPED_THRESHOLD_SQR = CLAMPED_THRESHOLD * CLAMPED_THRESHOLD;

SDL_Event event; 

void loadObject(const char* filename, Object* object, float scale) {
    object->triangle_count = 0;
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    
    object->triangle_count = file_size / sizeof(Triangle);
    object->triangles = (Triangle*)malloc(object->triangle_count * sizeof(Triangle));
    if (!object->triangles) {
        printf("Failed to allocate memory for triangles\n");
        fclose(file);
        return;
    }
    
    fread(object->triangles, sizeof(Triangle), object->triangle_count, file);
    fclose(file);

    // Scale the object
    for (size_t i = 0; i < object->triangle_count; i++) {
        for (int j = 0; j < 3; j++) { // Each vertex has 3 coordinates (x, y, z)
            object->triangles[i].v1[j] *= scale;
            object->triangles[i].v2[j] *= scale;
            object->triangles[i].v3[j] *= scale;
        }
    }
}

uint64_t addObject(const char* filename, float scale, float posX, float posY, float posZ, unsigned int color, uint8_t id) {
    
    // Expand objects list to fit one more object
    numObjects++;
    objects = (Object*)realloc(objects, numObjects * sizeof(Object));
    if (!objects) {
        printf("Failed to allocate memory for objects list\n");
        return -1;
    }
    
    uint64_t index = numObjects - 1;

    // Load the object's mesh data
    loadObject(filename, &objects[numObjects - 1], scale * 2);
    
    // Apply the X offset to all triangle vertices
    for (size_t i = 0; i < objects[numObjects - 1].triangle_count; i++) {
		objects[numObjects - 1].triangles[i].v1[0] += posX;
		objects[numObjects - 1].triangles[i].v2[0] += posX;
		objects[numObjects - 1].triangles[i].v3[0] += posX;
    }
    // Apply the Y offset to all triangle vertices
    for (size_t i = 0; i < objects[numObjects - 1].triangle_count; i++) {
		objects[numObjects - 1].triangles[i].v1[1] += posY;
		objects[numObjects - 1].triangles[i].v2[1] += posY;
		objects[numObjects - 1].triangles[i].v3[1] += posY;
    }
    // Apply the Z offset to all triangle vertices
    for (size_t i = 0; i < objects[numObjects - 1].triangle_count; i++) {
		objects[numObjects - 1].triangles[i].v1[2] += posZ;
		objects[numObjects - 1].triangles[i].v2[2] += posZ;
		objects[numObjects - 1].triangles[i].v3[2] += posZ;
    }    
    
    // Initialize all values
    objects[index].id = id;
    objects[index].color = color;
			
    objects[index].velX = 0;
    objects[index].velY = 0;
    objects[index].velZ = 0;
			
    objects[index].forward[0] = -1.0f;  // X-direction
	objects[index].forward[1] = 0.0f;  // Y-direction
	objects[index].forward[2] = 0.0f;  // Z-direction
			
	objects[index].up[0] = 0.0f;  // X-direction
	objects[index].up[1] = 1.0f;  // Y-direction
	objects[index].up[2] = 0.0f;  // Z-direction
			
	objects[index].right[0] = 0.0f;  // X-direction
	objects[index].right[1] = 0.0f;  // Y-direction
	objects[index].right[2] = 1.0f;  // Z-direction
	
	calculateObjectCenter(&objects[index], objects[index].position);
		
	objects[index].pathing.numDestinations = 0;
	if (id == 10) { // viper
		objects[index].pathing.destinations = (PathDestination*)malloc(sizeof(PathDestination));
		objects[index].parameters.minChaseDistance = 50.0f;
		objects[index].parameters.maxChaseDistance = 100000.0f;
		objects[index].parameters.forwardSpeed = 5.0f;
		objects[index].parameters.backwardSpeed = 3.0f;
		objects[index].parameters.rollSpeed = 0.1f;
		objects[index].parameters.pitchSpeed = 0.05f;
		objects[index].parameters.yawSpeed = 0.02f;
		objects[index].parameters.drag = 0.95f;
		objects[index].mob.personality = 0b10000000;
		objects[index].invincible = 0;
		objects[index].invisible = 0;
		objects[index].avoidanceRadius = 20;
		objects[index].mass = 1E2; 
		objects[index].parameters.drag = 0.95f;
	} else if (id == 0) { // cobra (player)
		objects[index].pathing.destinations = (PathDestination*)malloc(sizeof(PathDestination));
		objects[index].parameters.minChaseDistance = 50.0f;
		objects[index].parameters.maxChaseDistance = 1000.0f;
		objects[index].parameters.forwardSpeed = 7.0f;
		objects[index].parameters.backwardSpeed = 3.0f;
		objects[index].parameters.rollSpeed = 0.1f;
		objects[index].parameters.pitchSpeed = 0.03f;
		objects[index].parameters.yawSpeed = 0.02f;
		objects[index].parameters.drag = 0.95f;
		objects[index].mob.personality = 0b00000000;
		objects[index].invincible = 0;
		objects[index].invisible = 0;
		objects[index].avoidanceRadius = 30;
		objects[index].mass = 1E2;
		objects[index].parameters.drag = 0.95f;
	} else if (id == 1) { // planet
		objects[index].invincible = 1;
		objects[index].invisible = 0;
		objects[index].avoidanceRadius = scale + 50;
		objects[index].velZ = 0.0f;
		objects[index].mass = 1E6;
		objects[index].parameters.drag = 1;
		objects[index].planetIndex = numPlanets;
		numPlanets++;
		planets = (Planet*)realloc(planets, (numPlanets + 1) * sizeof(Planet));
	    if (!planets) {
	        printf("Failed to allocate memory for planet list\n");
	        return -1;
	    }
		planets[objects[index].planetIndex].spin = 1.0f;
	} else if (id == 2) { // star
		objects[index].invincible = 1;
		objects[index].invisible = 0;
		objects[index].avoidanceRadius = scale + 100;
		objects[index].mass = 1E18;
		objects[index].parameters.drag = 1;
		objects[index].starIndex = numStars;
		numStars++;
		stars = (Star*)realloc(stars, (numStars + 1) * sizeof(Star));
	    if (!stars) {
	        printf("Failed to allocate memory for star list\n");
	        return -1;
	    }
		stars[objects[index].starIndex].spin = 0.005 * M_PI / 180;
	}
	return index; // Return the index, so the caller can know directly what index was created
}

void removeObject(uint32_t index) {
	// First, free  up all the allocated memory
	free(objects[index].triangles);
	free(objects[index].pathing.destinations);
	
	// Don't 'remove' the object index, just set literally everything to 0
	objects[index].triangle_count = 0;
	
	objects[index].id = 0;
    objects[index].color = 0;
			
    objects[index].velX = 0;
    objects[index].velY = 0;
    objects[index].velZ = 0;
			
    objects[index].forward[0] = 0.0f;
	objects[index].forward[1] = 0.0f;
	objects[index].forward[2] = 0.0f;
			
	objects[index].up[0] = 0.0f;
	objects[index].up[1] = 0.0f;
	objects[index].up[2] = 0.0f;
			
	objects[index].right[0] = 0.0f;  
	objects[index].right[1] = 0.0f;  
	objects[index].right[2] = 0.0f;  
	
	// List it as available
	// todo ^
}

// Function to rotate a 3D point around the X-axis (pitch)
void rotateX(float point[3], float angle) {
    float y = point[1];
    float z = point[2];
    point[1] = y * cos(angle) - z * sin(angle);
    point[2] = y * sin(angle) + z * cos(angle);
}

// Function to rotate a 3D point around the Y-axis (yaw)
void rotateY(float point[3], float angle) {
    float x = point[0];
    float z = point[2];
    point[0] = x * cos(angle) + z * sin(angle);
    point[2] = -x * sin(angle) + z * cos(angle);
}

// Function to rotate a 3D point around the Z-axis (roll)
void rotateZ(float point[3], float angle) {
    float x = point[0];
    float y = point[1];
    point[0] = x * cos(angle) - y * sin(angle);
    point[1] = x * sin(angle) + y * cos(angle);
}

void rotateObject(Object* object, float pitch, float yaw, float roll) {
    if (!object || !object->triangles) return;

    // Compute object's center by averaging all triangle vertices
    float center[3] = {0.0f, 0.0f, 0.0f};

    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        center[0] += (tri->v1[0] + tri->v2[0] + tri->v3[0]) / 3.0f;
        center[1] += (tri->v1[1] + tri->v2[1] + tri->v3[1]) / 3.0f;
        center[2] += (tri->v1[2] + tri->v2[2] + tri->v3[2]) / 3.0f;
    }

    center[0] /= object->triangle_count;
    center[1] /= object->triangle_count;
    center[2] /= object->triangle_count;

    // Translate object to origin
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        tri->v1[0] -= center[0];
        tri->v1[1] -= center[1];
        tri->v1[2] -= center[2];

        tri->v2[0] -= center[0];
        tri->v2[1] -= center[1];
        tri->v2[2] -= center[2];

        tri->v3[0] -= center[0];
        tri->v3[1] -= center[1];
        tri->v3[2] -= center[2];
    }

    // Rotate around the local origin (center)
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        rotateX(tri->v1, pitch);
        rotateY(tri->v1, yaw);
        rotateZ(tri->v1, roll);

        rotateX(tri->v2, pitch);
        rotateY(tri->v2, yaw);
        rotateZ(tri->v2, roll);

        rotateX(tri->v3, pitch);
        rotateY(tri->v3, yaw);
        rotateZ(tri->v3, roll);
    }

    // Translate object back to its original position
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        tri->v1[0] += center[0];
        tri->v1[1] += center[1];
        tri->v1[2] += center[2];

        tri->v2[0] += center[0];
        tri->v2[1] += center[1];
        tri->v2[2] += center[2];

        tri->v3[0] += center[0];
        tri->v3[1] += center[1];
        tri->v3[2] += center[2];
    }

    // Rotate the forward vector
    rotateX(object->forward, pitch);
    rotateY(object->forward, yaw);
    rotateZ(object->forward, roll);
    
    rotateX(object->up, pitch);
    rotateY(object->up, yaw);
    rotateZ(object->up, roll);

    rotateX(object->right, pitch);
    rotateY(object->right, yaw);
    rotateZ(object->right, roll);
}

void rotateObjectAroundAxis(Object* object, float axis[3], float angle) {
    if (!object || !object->triangles) return;

    float cosA = cos(angle);
    float sinA = sin(angle);
    float oneMinusCosA = 1.0f - cosA;

    // Normalized axis components
    float ux = axis[0];
    float uy = axis[1];
    float uz = axis[2];

    // Rotation matrix for arbitrary axis
    float rotationMatrix[3][3] = {
        {
            cosA + ux * ux * oneMinusCosA,
            ux * uy * oneMinusCosA - uz * sinA,
            ux * uz * oneMinusCosA + uy * sinA
        },
        {
            uy * ux * oneMinusCosA + uz * sinA,
            cosA + uy * uy * oneMinusCosA,
            uy * uz * oneMinusCosA - ux * sinA
        },
        {
            uz * ux * oneMinusCosA - uy * sinA,
            uz * uy * oneMinusCosA + ux * sinA,
            cosA + uz * uz * oneMinusCosA
        }
    };

    // Compute object's center
    float center[3] = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];
        center[0] += (tri->v1[0] + tri->v2[0] + tri->v3[0]) / 3.0f;
        center[1] += (tri->v1[1] + tri->v2[1] + tri->v3[1]) / 3.0f;
        center[2] += (tri->v1[2] + tri->v2[2] + tri->v3[2]) / 3.0f;
    }
    center[0] /= object->triangle_count;
    center[1] /= object->triangle_count;
    center[2] /= object->triangle_count;

    // Function to apply rotation matrix to a point
    void applyRotation(float point[3], float rotationMatrix[3][3], float center[3]) {
        float temp[3] = { point[0] - center[0], point[1] - center[1], point[2] - center[2] };
        float rotated[3] = {
            temp[0] * rotationMatrix[0][0] + temp[1] * rotationMatrix[0][1] + temp[2] * rotationMatrix[0][2],
            temp[0] * rotationMatrix[1][0] + temp[1] * rotationMatrix[1][1] + temp[2] * rotationMatrix[1][2],
            temp[0] * rotationMatrix[2][0] + temp[1] * rotationMatrix[2][1] + temp[2] * rotationMatrix[2][2]
        };
        point[0] = rotated[0] + center[0];
        point[1] = rotated[1] + center[1];
        point[2] = rotated[2] + center[2];
    }

    // Rotate all vertices and orientation vectors
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        applyRotation(tri->v1, rotationMatrix, center);
        applyRotation(tri->v2, rotationMatrix, center);
        applyRotation(tri->v3, rotationMatrix, center);
    }

    applyRotation(object->forward, rotationMatrix, (float[3]){0, 0, 0});
    applyRotation(object->up, rotationMatrix, (float[3]){0, 0, 0});
    applyRotation(object->right, rotationMatrix, (float[3]){0, 0, 0});
}

void fnormalize(float v[3]) {
    float length = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0.0001f) { // Avoid division by zero
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}

// Convert an axis-angle representation to a quaternion
Quaternion axisAngleToQuaternion(float axis[3], float angle) {
    Quaternion q;
    float halfAngle = angle * 0.5f;
    float sinHalfAngle, cosHalfAngle;

    sincosf(halfAngle, &sinHalfAngle, &cosHalfAngle); // Single call for sin/cos

    // Normalize inline
    float len = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (len > 0.0f) {
        float invLen = 1.0f / len;
        axis[0] *= invLen;
        axis[1] *= invLen;
        axis[2] *= invLen;
    }

    q.w = cosHalfAngle;
    q.x = axis[0] * sinHalfAngle;
    q.y = axis[1] * sinHalfAngle;
    q.z = axis[2] * sinHalfAngle;

    return q;
}

Quaternion rotationMatrixToQuaternion(float forward[3], float up[3], float right[3]) {
    Quaternion q;
    float trace = right[0] + up[1] + forward[2]; // Matrix trace

    if (trace > 0.0f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (up[2] - forward[1]) * s;
        q.y = (forward[0] - right[2]) * s;
        q.z = (right[1] - up[0]) * s;
    } else {
        float s;
        if (right[0] > up[1] && right[0] > forward[2]) {
            s = 2.0f * sqrtf(1.0f + right[0] - up[1] - forward[2]);
            q.w = (up[2] - forward[1]) / s;
            q.x = 0.25f * s;
            q.y = (up[0] + right[1]) / s;
            q.z = (forward[0] + right[2]) / s;
        } else if (up[1] > forward[2]) {
            s = 2.0f * sqrtf(1.0f + up[1] - right[0] - forward[2]);
            q.w = (forward[0] - right[2]) / s;
            q.x = (up[0] + right[1]) / s;
            q.y = 0.25f * s;
            q.z = (forward[1] + up[2]) / s;
        } else {
            s = 2.0f * sqrtf(1.0f + forward[2] - right[0] - up[1]);
            q.w = (right[1] - up[0]) / s;
            q.x = (forward[0] + right[2]) / s;
            q.y = (forward[1] + up[2]) / s;
            q.z = 0.25f * s;
        }
    }

    return q;
}

Quaternion rotationBetweenVectors(float v1[3], float v2[3]) {
    // Normalize vectors inline
    float len1 = sqrtf(v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2]);
    float len2 = sqrtf(v2[0] * v2[0] + v2[1] * v2[1] + v2[2] * v2[2]);

    if (len1 < 1e-6f || len2 < 1e-6f) return (Quaternion){1.0f, 0.0f, 0.0f, 0.0f};

    float invLen1 = 1.0f / len1;
    float invLen2 = 1.0f / len2;

    v1[0] *= invLen1; v1[1] *= invLen1; v1[2] *= invLen1;
    v2[0] *= invLen2; v2[1] *= invLen2; v2[2] *= invLen2;

    float dot = v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];

    if (dot > 0.9999f) return (Quaternion){1.0f, 0.0f, 0.0f, 0.0f};
    if (dot < -0.9999f) { // Opposite vectors — pick an orthogonal axis
        float orthogonal[3] = {1.0f, 0.0f, 0.0f};
        if (fabsf(v1[0]) > 0.9f) orthogonal[0] = 0.0f, orthogonal[2] = 1.0f;

        float axis[3] = { v1[1] * orthogonal[2] - v1[2] * orthogonal[1],
                          v1[2] * orthogonal[0] - v1[0] * orthogonal[2],
                          v1[0] * orthogonal[1] - v1[1] * orthogonal[0] };
        
        return axisAngleToQuaternion(axis, (float)M_PI);
    }

    float axis[3] = { v1[1] * v2[2] - v1[2] * v2[1], 
                      v1[2] * v2[0] - v1[0] * v2[2], 
                      v1[0] * v2[1] - v1[1] * v2[0] };

    return axisAngleToQuaternion(axis, acosf(dot));
}

// Function to rotate a point using a quaternion
void rotatePointByQuaternion(float point[3], Quaternion q) {
    float u[3] = { q.x, q.y, q.z };
    float s = q.w;

    float uDotP = u[0] * point[0] + u[1] * point[1] + u[2] * point[2];
    float uCrossP[3] = { u[1] * point[2] - u[2] * point[1],
                         u[2] * point[0] - u[0] * point[2],
                         u[0] * point[1] - u[1] * point[0] };

    point[0] = 2.0f * uDotP * u[0] + (s * s - (u[0] * u[0] + u[1] * u[1] + u[2] * u[2])) * point[0] + 2.0f * s * uCrossP[0];
    point[1] = 2.0f * uDotP * u[1] + (s * s - (u[0] * u[0] + u[1] * u[1] + u[2] * u[2])) * point[1] + 2.0f * s * uCrossP[1];
    point[2] = 2.0f * uDotP * u[2] + (s * s - (u[0] * u[0] + u[1] * u[1] + u[2] * u[2])) * point[2] + 2.0f * s * uCrossP[2];
}

// Function to rotate the entire object with a quaternion
void rotateObjectByQuaternion(Object* object, Quaternion q) {
    if (!object || !object->triangles) return;

    // Rotate orientation vectors
    rotatePointByQuaternion(object->forward, q);
    rotatePointByQuaternion(object->up, q);
    rotatePointByQuaternion(object->right, q);

    // Compute object center
    float center[3] = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];
        center[0] += (tri->v1[0] + tri->v2[0] + tri->v3[0]) / 3.0f;
        center[1] += (tri->v1[1] + tri->v2[1] + tri->v3[1]) / 3.0f;
        center[2] += (tri->v1[2] + tri->v2[2] + tri->v3[2]) / 3.0f;
    }
    center[0] /= object->triangle_count;
    center[1] /= object->triangle_count;
    center[2] /= object->triangle_count;

    // Rotate all vertices around the object center
    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        // Move vertices to origin, rotate, then move back
        for (int v = 0; v < 3; v++) {
            float* vertex = (v == 0) ? tri->v1 : (v == 1) ? tri->v2 : tri->v3;

            // Translate to origin
            vertex[0] -= center[0];
            vertex[1] -= center[1];
            vertex[2] -= center[2];

            // Rotate
            rotatePointByQuaternion(vertex, q);

            // Translate back
            vertex[0] += center[0];
            vertex[1] += center[1];
            vertex[2] += center[2];
        }
    }
}

// Spherical Linear Interpolation (SLERP) between two quaternions
Quaternion slerp(Quaternion q1, Quaternion q2, float t) {
    // Clamp t to [0, 1]
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // Compute the dot product (cosine of the angle)
    float dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;

    // If the dot product is negative, slerp the opposite side to avoid spinning
    if (dot < 0.0f) {
        q2.w = -q2.w;
        q2.x = -q2.x;
        q2.y = -q2.y;
        q2.z = -q2.z;
        dot = -dot;
    }

    // If the quaternions are very close, use linear interpolation
    const float SLERP_THRESHOLD = 0.9995f;
    if (dot > SLERP_THRESHOLD) {
        Quaternion result = {
            q1.w + t * (q2.w - q1.w),
            q1.x + t * (q2.x - q1.x),
            q1.y + t * (q2.y - q1.y),
            q1.z + t * (q2.z - q1.z)
        };
        float length = sqrt(result.w * result.w + result.x * result.x + result.y * result.y + result.z * result.z);
        result.w /= length;
        result.x /= length;
        result.y /= length;
        result.z /= length;
        return result;
    }

    // Calculate the interpolation
    float theta_0 = acos(dot);              // Angle between quaternions
    float theta = theta_0 * t;              // Interpolated angle
    float sin_theta = sin(theta);
    float sin_theta_0 = sin(theta_0);

    float s0 = cos(theta) - dot * sin_theta / sin_theta_0;
    float s1 = sin_theta / sin_theta_0;

    Quaternion result = {
        s0 * q1.w + s1 * q2.w,
        s0 * q1.x + s1 * q2.x,
        s0 * q1.y + s1 * q2.y,
        s0 * q1.z + s1 * q2.z
    };

    return result;
}

// Function to smoothly rotate an object toward a target point in space
void turnTowardsPoint(Object* object, float target[3]) {
    if (!object) return;

    // Compute direction to target
    float direction[3] = {
        target[0] - object->position[0],
        target[1] - object->position[1],
        target[2] - object->position[2]
    };
    fnormalize(direction);

    // Get the rotation needed to align forward vector with target direction
    Quaternion targetRotation = rotationBetweenVectors(object->forward, direction);

    // Calculate the angle between current forward and target direction
    float dotProduct = object->forward[0] * direction[0] +
                       object->forward[1] * direction[1] +
                       object->forward[2] * direction[2];
    dotProduct = fmax(fmin(dotProduct, 1.0f), -1.0f); // Clamp to avoid NaNs

    float angle = acos(dotProduct);

    // Clamp the rotation angle to the max average turn rate
    float maxRotation = (object->parameters.yawSpeed + object->parameters.pitchSpeed + object->parameters.rollSpeed) /3;
    float t = fmin(1.0f, maxRotation / angle);

    // Interpolate using the clamped factor
    Quaternion smoothedRotation = slerp((Quaternion){ 1.0f, 0.0f, 0.0f, 0.0f }, targetRotation, t);

    // Rotate the entire object, including vertices and orientation vectors
    rotateObjectByQuaternion(object, smoothedRotation);
}

// Quaternion Multiplication
Quaternion multiplyQuat(Quaternion q1, Quaternion q2) {
     return (Quaternion){
        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    };
}

// Convert Axis-Angle to Quaternion
Quaternion axisAngleToQuat(Vec3 axis, float angle) {
    float halfAngle = angle * 0.5f;
    float sinHalf = sin(halfAngle);
    return (Quaternion){cos(halfAngle), axis.x * sinHalf, axis.y * sinHalf, axis.z * sinHalf};
}

// Rotate Quaternion by Axis
void rotateCamera(Vec3 axis, float angle) {
    Quaternion rotation = axisAngleToQuat(axis, angle);
    cameraOrientation = multiplyQuat(rotation, cameraOrientation);
}

// Apply Quaternion to a Vector (Rotate Vector)
Vec3 rotateVecByQuat(Vec3 v, Quaternion q) {
    Quaternion qConj = {q.w, -q.x, -q.y, -q.z};
    Quaternion vQuat = {0, v.x, v.y, v.z};
    Quaternion result = multiplyQuat(multiplyQuat(q, vQuat), qConj);
    return (Vec3){result.x, result.y, result.z};
}

Vec3 crossProduct(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalize(Vec3 v) {
    float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length == 0) return (Vec3){0, 0, 0};
    return (Vec3){v.x / length, v.y / length, v.z / length};
}

Quaternion normalizeQuaternion(Quaternion q) {
    float mag = sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return (Quaternion){q.w / mag, q.x / mag, q.y / mag, q.z / mag};
}

void addRandomPerturbation(float vector[3], float strength) {
    vector[0] += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * strength;
    vector[1] += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * strength;
    vector[2] += ((float)rand() / RAND_MAX * 2.0f - 1.0f) * strength;
}

// Move an object, yes it's practically hijacked from the loadObject function
void moveObject(Object* object, float moveX, float moveY, float moveZ) {
    // Apply the X movement to all triangle vertices
    for (size_t i = 0; i < object->triangle_count; i++) {
		object->triangles[i].v1[0] += moveX;
		object->triangles[i].v2[0] += moveX;
		object->triangles[i].v3[0] += moveX;
    }
    // Apply the Y movement to all triangle vertices
    for (size_t i = 0; i < object->triangle_count; i++) {
		object->triangles[i].v1[1] += moveY;
		object->triangles[i].v2[1] += moveY;
		object->triangles[i].v3[1] += moveY;
    }
    // Apply the Z movement to all triangle vertices
    for (size_t i = 0; i < object->triangle_count; i++) {
		object->triangles[i].v1[2] += moveZ;
		object->triangles[i].v2[2] += moveZ;
		object->triangles[i].v3[2] += moveZ;
    }
}

void saveToBMP(unsigned char* pixels, const char* filename) {
    // Define the file header structure (14 bytes)
    uint8_t fileHeader[14] = {
        'B', 'M',           // BM signature
        0, 0, 0, 0,         // File size (to be filled later)
        0, 0,               // Reserved
        0, 0,               // Reserved
        54, 0, 0, 0         // Data offset (54 bytes for header)
    };

    // Define the info header structure (40 bytes)
    uint8_t infoHeader[40] = {
        40, 0, 0, 0,        // Header size (40 bytes)
        0, 0, 0, 0,         // Width (to be filled later)
        0, 0, 0, 0,         // Height (to be filled later)
        1, 0,               // Color planes (1)
        24, 0,              // Bits per pixel (24 bits = 3 bytes)
        0, 0, 0, 0,         // Compression (no compression)
        0, 0, 0, 0,         // Image size (0 for uncompressed)
        0, 0, 0, 0,         // Horizontal resolution (not used)
        0, 0, 0, 0,         // Vertical resolution (not used)
        0, 0, 0, 0          // Number of colors (0 = default)
    };

    // Set the width and height in the info header (in little-endian format)
    infoHeader[4] = SCREEN_WIDTH & 0xFF;
    infoHeader[5] = (SCREEN_WIDTH >> 8) & 0xFF;
    infoHeader[6] = (SCREEN_WIDTH >> 16) & 0xFF;
    infoHeader[7] = (SCREEN_WIDTH >> 24) & 0xFF;

    infoHeader[8] = SCREEN_HEIGHT & 0xFF;
    infoHeader[9] = (SCREEN_HEIGHT >> 8) & 0xFF;
    infoHeader[10] = (SCREEN_HEIGHT >> 16) & 0xFF;
    infoHeader[11] = (SCREEN_HEIGHT >> 24) & 0xFF;

    // Calculate the row size (padded to a multiple of 4 bytes)
    int rowSize = (SCREEN_WIDTH * 3 + 3) & ~3;  // Round up to multiple of 4
    int imageSize = rowSize * SCREEN_HEIGHT;

    // Update file size and image size in headers
    fileHeader[2] = (imageSize + 54) & 0xFF;
    fileHeader[3] = ((imageSize + 54) >> 8) & 0xFF;
    fileHeader[4] = ((imageSize + 54) >> 16) & 0xFF;
    fileHeader[5] = ((imageSize + 54) >> 24) & 0xFF;

    // Open the file for writing
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Error opening file for writing\n");
        return;
    }

    // Write the file and info headers
    fwrite(fileHeader, sizeof(uint8_t), 14, file);
    fwrite(infoHeader, sizeof(uint8_t), 40, file);

    // Write pixel data
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            // Get pixel color
            unsigned char* pixel = &pixels[(y * SCREEN_WIDTH + x) * 3];
            
            // Write RGB (pixel[2] is Red, pixel[1] is Green, pixel[0] is Blue)
            fwrite(&pixel[2], sizeof(uint8_t), 1, file);  // Red
            fwrite(&pixel[1], sizeof(uint8_t), 1, file);  // Green
            fwrite(&pixel[0], sizeof(uint8_t), 1, file);  // Blue
        }

        // Write padding for the row if necessary (to ensure it is a multiple of 4 bytes)
        int padding = rowSize - SCREEN_WIDTH * 3;
        for (int i = 0; i < padding; ++i) {
            fputc(0, file);  // Write padding byte (0)
        }
    }

    // Close the file
    fclose(file);
    printf("Image saved to %s\n", filename);
}

void handleInput(unsigned char* pixels) {
    // Handle exit events, proably better to include this than to not
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            running = 0;
        }
    }
    
    const Uint8* state = SDL_GetKeyboardState(NULL);
    
    // Direction vectors
    Vec3 forward = rotateVecByQuat((Vec3){0, 0, -1}, cameraOrientation);
    Vec3 right = rotateVecByQuat((Vec3){1, 0, 0}, cameraOrientation);
    Vec3 up = rotateVecByQuat((Vec3){0, 1, 0}, cameraOrientation);
    
    // todo: review controls, make sure they make sense/are feasable
    if (state[SDL_SCANCODE_W]) {
        objects[0].velX += objects[0].forward[0] * MOVEMENT_DAMPENING * objects[0].parameters.forwardSpeed;
        objects[0].velY += objects[0].forward[1] * MOVEMENT_DAMPENING * objects[0].parameters.forwardSpeed;
        objects[0].velZ += objects[0].forward[2] * MOVEMENT_DAMPENING * objects[0].parameters.forwardSpeed;
    }
    if (state[SDL_SCANCODE_S]) {
        objects[0].velX += -objects[0].forward[0] * MOVEMENT_DAMPENING * objects[0].parameters.backwardSpeed;
        objects[0].velY += -objects[0].forward[1] * MOVEMENT_DAMPENING * objects[0].parameters.backwardSpeed;
        objects[0].velZ += -objects[0].forward[2] * MOVEMENT_DAMPENING * objects[0].parameters.backwardSpeed;
    }
    if (state[SDL_SCANCODE_A]) {
	    rotateObjectAroundAxis(&objects[0], objects[0].up, objects[0].parameters.yawSpeed);  // Yaw left (negative yaw)
	}
	
	if (state[SDL_SCANCODE_D]) {
	    rotateObjectAroundAxis(&objects[0], objects[0].up, -objects[0].parameters.yawSpeed);  // Yaw right (positive yaw)
	}
	
    if (state[SDL_SCANCODE_Q]) {
	    rotateObjectAroundAxis(&objects[0], objects[0].forward, -objects[0].parameters.pitchSpeed);  
	}
	if (state[SDL_SCANCODE_E]) {
	    rotateObjectAroundAxis(&objects[0], objects[0].forward, objects[0].parameters.pitchSpeed);
	}
	
	if (state[SDL_SCANCODE_R]) {
	    rotateObjectAroundAxis(&objects[0], objects[0].right, -objects[0].parameters.pitchSpeed);  
	}
	if (state[SDL_SCANCODE_F]) {
	    rotateObjectAroundAxis(&objects[0], objects[0].right, objects[0].parameters.pitchSpeed);
	}
	
	// Camera controls
	if (state[SDL_SCANCODE_I]) {
        cameraPos.x += forward.x * cameraSpeed;
        cameraPos.y += forward.y * cameraSpeed;
        cameraPos.z += forward.z * cameraSpeed;
    }
    if (state[SDL_SCANCODE_K]) {
        cameraPos.x -= forward.x * cameraSpeed;
        cameraPos.y -= forward.y * cameraSpeed;
        cameraPos.z -= forward.z * cameraSpeed;
    }
    if (state[SDL_SCANCODE_J]) {
        cameraPos.x -= right.x * cameraSpeed;
        cameraPos.y -= right.y * cameraSpeed;
        cameraPos.z -= right.z * cameraSpeed;
    }
    if (state[SDL_SCANCODE_L]) {
        cameraPos.x += right.x * cameraSpeed;
        cameraPos.y += right.y * cameraSpeed;
        cameraPos.z += right.z * cameraSpeed;
    }
    if (state[SDL_SCANCODE_LEFT]) rotateCamera(up, TURN_SPEED);      // Yaw left
    if (state[SDL_SCANCODE_RIGHT]) rotateCamera(up, -TURN_SPEED);    // Yaw right
    if (state[SDL_SCANCODE_UP]) rotateCamera(right, TURN_SPEED);     // Pitch up
    if (state[SDL_SCANCODE_DOWN]) rotateCamera(right, -TURN_SPEED);  // Pitch down
    if (state[SDL_SCANCODE_U]) rotateCamera(forward, -TURN_SPEED);   // Roll left
    if (state[SDL_SCANCODE_O]) rotateCamera(forward, TURN_SPEED);    // Roll right
    
    if (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_SPACE]) {
        cameraPos.x += up.x * cameraSpeed;
        cameraPos.y += up.y * cameraSpeed;
        cameraPos.z += up.z * cameraSpeed;
    }
    if (state[SDL_SCANCODE_LCTRL]) {
        cameraPos.x -= up.x * cameraSpeed;
        cameraPos.y -= up.y * cameraSpeed;
        cameraPos.z -= up.z * cameraSpeed;
    }
    
    Uint32 currentTime = SDL_GetTicks(); // Get current time in milliseconds
    
    if (state[SDL_SCANCODE_Z] && (currentTime - firstPersonTime >= 1000)) {
		firstPerson = firstPerson ? 0 : 1;
		freeLook = (firstPerson - 1) % 1;
		objects[0].invisible = firstPerson;
		firstPersonTime = currentTime; // Update the last execution time
	}
	if (state[SDL_SCANCODE_X] && (currentTime - freeLookTime >= 1000)) {
		freeLook = freeLook ? 0 : 1;
		freeLookTime = currentTime; // Update the last execution time
	}
	
	if (state[SDL_SCANCODE_P] && (currentTime - pauseTime >= 1000)) {
		paused = paused ? 0 : 1;
		pauseTime = currentTime; // Update the last execution time
	}
	if (state[SDL_SCANCODE_0] && (currentTime - pauseTime >= 100)) {
		saveToBMP(pixels, "output.bmp");
		pauseTime = currentTime; // Update the last execution time
	}
}

// Given a point, project it to 2D screen space.
// Returns 1 if the vertex is visible, even if some vertices are behind the camera.
int projectVertex(const float world[3], float* screenX, float* screenY) {
    // Load world coordinates and camera position
    __m128 worldPos = _mm_set_ps(0, world[2], world[1], world[0]);
    __m128 camPos   = _mm_set_ps(0, cameraPos.z, cameraPos.y, cameraPos.x);

    // Compute dx, dy, dz = world - cameraPos
    __m128 diff = _mm_sub_ps(worldPos, camPos);

    // Load camera basis vectors
    __m128 camRightVec   = _mm_set_ps(0, camRight.z, camRight.y, camRight.x);
    __m128 camUpVec      = _mm_set_ps(0, camUp.z, camUp.y, camUp.x);
    __m128 camForwardVec = _mm_set_ps(0, camForward.z, camForward.y, camForward.x);

    // Compute dot products for camera space transformation
    float x_cam = _mm_cvtss_f32(_mm_dp_ps(diff, camRightVec, 0x71));  // Mask 0x71 ensures XYZ dot product
    float y_cam = _mm_cvtss_f32(_mm_dp_ps(diff, camUpVec, 0x71));
    float z_cam = _mm_cvtss_f32(_mm_dp_ps(diff, camForwardVec, 0x71));

    // If point is behind the camera, return 0
    if (z_cam <= 0) {
        return 0;
    }

    // Compute projection
    float inv_z = 1.0f / z_cam;
    *screenX = (x_cam * inv_z) * f + SCREEN_WIDTH / 2.0f;
    *screenY = (y_cam * inv_z) * f + SCREEN_HEIGHT / 2.0f;

    return 1;
}

void drawEdge(float x0, float y0, float x1, float y1, uint8_t* pixels, uint32_t color) {
    // Convert coordinates to integers
    int ix0 = (int)(x0 + 0.5f);
    int iy0 = (int)(y0 + 0.5f);
    int ix1 = (int)(x1 + 0.5f);
    int iy1 = (int)(y1 + 0.5f);

    // Extract color components
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Compute line deltas
    int dx = abs(ix1 - ix0);
    int dy = abs(iy1 - iy0);
    int sx = ix0 < ix1 ? 1 : -1;
    int sy = iy0 < iy1 ? 1 : -1;

    // Error term
    int err = dx - dy;

    // Draw line
    while (1) {
        int pixelIndex = (iy0 * SCREEN_WIDTH + ix0) * 3;
        if (ix0 < 0 || ix0 >= SCREEN_WIDTH || iy0 < 0 || iy0 >= SCREEN_HEIGHT) {
		    break;
		}
		

        // Store the color in the pixel buffer
        pixels[pixelIndex] = r;
        pixels[pixelIndex + 1] = g;
        pixels[pixelIndex + 2] = b;

        if (ix0 == ix1 && iy0 == iy1) break;
        
        // Update error term and pixel position
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            ix0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            iy0 += sy;
        }
    }
}

void freeObjects() {
    // Free all dynamically allocated triangles
    for (size_t i = 0; i < numObjects; i++) {
        free(objects[i].triangles);
    }
    free(objects);
    objects = NULL;
    numObjects = 0;
}

void generateSkyboxStars(float starOffset[3]) {
	srand(STAR_SEED);
    for (int i = 0; i < SKYBOXSTAR_COUNT; i++) {
		while (fabs(SkyboxStars[i].position[0]) < SKYBOXSTAR_MIN_DISTANCE &&
		       fabs(SkyboxStars[i].position[1]) < SKYBOXSTAR_MIN_DISTANCE &&
		       fabs(SkyboxStars[i].position[2]) < SKYBOXSTAR_MIN_DISTANCE) {
	        // Random radius, spread more uniformly by taking cube root
	        float radius = cbrt((float)rand() / RAND_MAX) * SKYBOXSTAR_MAX_DISTANCE; // Cube root for uniform density
	
	        // Random angle around the Y-axis (from 0 to 2π)
	        float theta = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
	
	        // Random angle from top to bottom (from 0 to π) — uniform distribution across vertical axis
	        float phi = acos(1.0f - 2.0f * (float)rand() / RAND_MAX);
	
	        // Convert spherical coordinates to Cartesian coordinates
	        SkyboxStars[i].position[0] = starOffset[0] + radius * sin(phi) * cos(theta); // X
	        SkyboxStars[i].position[1] = starOffset[1] + radius * sin(phi) * sin(theta); // Y
	        SkyboxStars[i].position[2] = starOffset[2] + radius * cos(phi);              // Z
		}
		
        // Weighted random choice based on SkyboxStar type distribution
        int SkyboxStarType = rand() % 100;

        uint8_t r, g, b;
        float size;

		// SkyboxStar distribution is NOT realistic, I just tuned it for a while until I liked it
		
        // Main Sequence SkyboxStars
        if (SkyboxStarType < 35) {  // Main Sequence (Yellow)
            r = 255;
            g = 255;
            b = 255;//rand() % 100 + 100; 
            size = (float)(rand() % 80 + 30) * 1.0f; // Medium size for yellow SkyboxStars

        // Red Dwarfs
        } else if (SkyboxStarType < 60) {  // Red Dwarfs (Red)
            r = 255;
            g = 255;//rand() % 200; 
            b = 255;//rand() % 200;
            size = (float)(rand() % 50 + 10) * 0.8f; // Smaller size for cooler SkyboxStars

        // Blue Supergiants
        } else if (SkyboxStarType < 80) {  // Blue Supergiants (Blue)
            r = 255;//50 + rand() % 150; 
            g = 255;//50 + rand() % 50;
            b = 255; 
            size = (float)(rand() % 100 + 50) * 2.5f; // Larger size for hotter SkyboxStars

        // Other types (5% chance) — can include Giants, White Dwarfs, etc.
        } else {  // White Dwarfs, Giants, etc.
            int otherType = rand() % 3;
            if (otherType == 0) {  // White Dwarf (pale blue/white)
                r = 255;//200 + rand() % 55;
                g = 255;//200 + rand() % 55;
                b = 255;
                size = (float)(rand() % 20 + 5) * 0.5f; // Very small size for white dwarfs
            } else if (otherType == 1) {  // Giant star (Orange)
                r = 255;
                g = 255;//165;
                b = 255;//50;
                size = (float)(rand() % 120 + 60) * 1.5f; // Larger size for giant SkyboxStars
            } else {  // Red Giant (Red)
                r = 255;
                g = 255;//99;
                b = 255;//71; 
                size = (float)(rand() % 200 + 80) * 2.0f; // Very large for red giants
            }
        }

        SkyboxStars[i].color = ((int)(r) << 16) | ((int)(g) << 8) | (int)(b);
        SkyboxStars[i].size = size/200;  // Store the size of the SkyboxStar
    }
}

void drawSkyboxStar(float x, float y, unsigned char* pixels, unsigned int color, float size) {
    int screenX = (int)x;
    int screenY = (int)y;
    int radius = (int)size;

    // Check if the center of the SkyboxStar is within screen bounds
    if (screenX - radius < 0 || screenX + radius >= SCREEN_WIDTH || screenY - radius < 0 || screenY + radius >= SCREEN_HEIGHT)
        return;

    // Loop over the pixels in the square that bounds the circle
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Check if the pixel is inside the circle (distance from center less than radius)
            if (dx * dx + dy * dy <= radius * radius) {
                int drawX = screenX + dx;
                int drawY = screenY + dy;

                // Make sure the pixel is within the screen bounds
                if (drawX >= 0 && drawX < SCREEN_WIDTH && drawY >= 0 && drawY < SCREEN_HEIGHT) {
                    int index = (drawY * SCREEN_WIDTH + drawX) * 3;

                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;

                    // Set the pixel color
                    pixels[index] = r;
                    pixels[index + 1] = g;
                    pixels[index + 2] = b;
                }
            }
        }
    }
}

void drawSkyboxStars(unsigned char* pixels) {
    for (int i = 0; i < SKYBOXSTAR_COUNT; i++) {
        float screenX, screenY;
        if (projectVertex(SkyboxStars[i].position, &screenX, &screenY)) {
            drawSkyboxStar(screenX, screenY, pixels, SkyboxStars[i].color, SkyboxStars[i].size);
        }
    }
}

void drawVector(float center[3], float vector[3], unsigned char* pixels, float length, unsigned int color) {
    if (!center || !vector || !pixels) return;

    // Compute the endpoint of the vector
    float endPoint[3] = {
        center[0] + vector[0] * length,
        center[1] + vector[1] * length,
        center[2] + vector[2] * length
    };

    float screenX1, screenY1, screenX2, screenY2;
    if (!projectVertex(center, &screenX1, &screenY1)) return;
    if (!projectVertex(endPoint, &screenX2, &screenY2)) return;

    // Draw the vector line
    drawEdge(screenX1, screenY1, screenX2, screenY2, pixels, color);
}

void calculateObjectCenter(const Object* object, float center[3]) {
    if (!object || !object->triangles || object->triangle_count == 0) return;

    center[0] = 0.0f;
    center[1] = 0.0f;
    center[2] = 0.0f;

    size_t totalVertices = object->triangle_count * 3;

    for (size_t i = 0; i < object->triangle_count; i++) {
        Triangle* tri = &object->triangles[i];

        center[0] += tri->v1[0] + tri->v2[0] + tri->v3[0];
        center[1] += tri->v1[1] + tri->v2[1] + tri->v3[1];
        center[2] += tri->v1[2] + tri->v2[2] + tri->v3[2];
    }

    // Divide by the total number of vertices to get the average
    center[0] /= totalVertices;
    center[1] /= totalVertices;
    center[2] /= totalVertices;
}

// Calculate distance between two points float version
static inline float fgetDistance3D(const float *a, const float *b) {
    // Load 3D vectors into SIMD registers (X, Y, Z, ?)  
    __m128 va = _mm_loadu_ps(a);  // Load (a[0], a[1], a[2], ?)  
    __m128 vb = _mm_loadu_ps(b);  // Load (b[0], b[1], b[2], ?)  

    // Compute (b - a)
    __m128 diff = _mm_sub_ps(vb, va);

    // Square each component (diff * diff)
    __m128 squared = _mm_mul_ps(diff, diff);

    // Sum all components using horizontal add
    __m128 sum1 = _mm_hadd_ps(squared, squared);
    __m128 sum2 = _mm_hadd_ps(sum1, sum1);

    // Compute the square root of the sum
    return _mm_cvtss_f32(_mm_sqrt_ss(sum2));
}

// Comparison function for sorting
int compareByDistance(const void* a, const void* b) {
    float distA = ((DrawableDistance*)a)->distance;
    float distB = ((DrawableDistance*)b)->distance;
    return (distB > distA) - (distB < distA);  // Sort descending
}

bool isBackface(float v1[3], float v2[3], float v3[3], float cameraPos[3]) {
    // Calculate the normal vector directly using the cross product
    float normalX = (v2[1] - v1[1]) * (v3[2] - v1[2]) - (v2[2] - v1[2]) * (v3[1] - v1[1]);
    float normalY = (v2[2] - v1[2]) * (v3[0] - v1[0]) - (v2[0] - v1[0]) * (v3[2] - v1[2]);
    float normalZ = (v2[0] - v1[0]) * (v3[1] - v1[1]) - (v2[1] - v1[1]) * (v3[0] - v1[0]);

    // Calculate the view direction directly
    float viewDirX = cameraPos[0] - v1[0];
    float viewDirY = cameraPos[1] - v1[1];
    float viewDirZ = cameraPos[2] - v1[2];

    // Dot product between normal and view direction
    float dotProduct = normalX * viewDirX + normalY * viewDirY + normalZ * viewDirZ;

    return dotProduct > 0; // If positive, the face is facing away
}

// Function to calculate the dot product of two 3D vectors
float dotProduct(float v1[3], float v2[3]) {
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

// Shade color based on light direction and surface normal
uint32_t shadeColor(uint32_t color, float v1[3], float v2[3], float v3[3], float lightPos[3]) {
    // Step 1: Compute two edges of the triangle
    float edge1[3] = {v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2]};
    float edge2[3] = {v3[0] - v1[0], v3[1] - v1[1], v3[2] - v1[2]};

    // Step 2: Calculate the surface normal using the cross product
    float normal[3] = {
        edge1[1] * edge2[2] - edge1[2] * edge2[1],  // normal.x
        edge1[2] * edge2[0] - edge1[0] * edge2[2],  // normal.y
        edge1[0] * edge2[1] - edge1[1] * edge2[0]   // normal.z
    };

    // Step 3: Normalize the normal vector
    fnormalize(normal);

    // Step 4: Compute the vector from the triangle to the light source
    float lightDir[3] = {lightPos[0] - v1[0], lightPos[1] - v1[1], lightPos[2] - v1[2]};

    // Step 5: Normalize the light direction
    fnormalize(lightDir);

    // Step 6: Calculate the dot product between the normal and light direction
    float dot = dotProduct(normal, lightDir);

    // Step 7: Ensure the dot product is between 0 and 1 (clamp it)
    dot = fmaxf(dot, 0.0f);  // If the light is behind, we get no light (shadow).

    // Step 8: Extract the color components from the 32-bit color
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Step 9: Apply shading based on the dot product (light intensity)
    r = (uint8_t)(r * dot);
    g = (uint8_t)(g * dot);
    b = (uint8_t)(b * dot);

    // Step 10: Combine the shaded color back into a single 32-bit value
    uint32_t shadedColor = (r << 16) | (g << 8) | b;

    return shadedColor;
}

void renderScene(unsigned char* pixels) {
    float cameraPosition[3] = {cameraPos.x, cameraPos.y, cameraPos.z};
    int totalItems = 0; // Fix: Track valid entries
    DrawableDistance drawQueue[numObjects];

    // Populate drawQueue only with visible objects
    for (int j = 0; j < numObjects; j++) {
        if (objects[j].id == 255 || objects[j].invisible) continue; // Skip invisible objects

        // Store in drawQueue only if it's visible
        drawQueue[totalItems].distance = fgetDistance3D(cameraPosition, objects[j].position);
        drawQueue[totalItems].index = j;
        totalItems++; // Fix: Increment only for valid objects
    }

    // Ensure sorting only happens for valid entries
    if (totalItems > 1) {
        qsort(drawQueue, totalItems, sizeof(DrawableDistance), compareByDistance);
    }

	float* lightPos = objects[2].position;  // Light position (example)
	uint32_t objectColor = 0xFF00FF;  // Magenta color (RGB: 255, 0, 255)
	
    // Render in sorted order
    for (int i = 0; i < totalItems; i++) {
        int objIndex = drawQueue[i].index;
        float objectCenter[3] = {0.0f, 0.0f, 0.0f};
        calculateObjectCenter(&objects[objIndex], objectCenter);

        if (!firstPerson) {
            //drawVector(objectCenter, objects[objIndex].forward, pixels, 10.0f, 0xFF0000);
            //drawVector(objectCenter, objects[objIndex].right, pixels, 10.0f, 0x00FF00);
            //drawVector(objectCenter, objects[objIndex].up, pixels, 10.0f, 0x0000FF);
        }
        
        unsigned int shadedColor = objects[objIndex].color;  // Default color
		
		//#pragma omp parallel for num_threads(4) schedule(dynamic)
        for (int k = 0; k < objects[objIndex].triangle_count; k++) {
			if (!isBackface(objects[objIndex].triangles[k].v1, objects[objIndex].triangles[k].v2, objects[objIndex].triangles[k].v3, cameraPosition)) {
			    //continue;
			}
			uint32_t shadedColor = objects[objIndex].color;
			if ((objects[objIndex].id == 1)) {
				shadedColor = shadeColor(objects[objIndex].color, objects[objIndex].triangles[k].v1, objects[objIndex].triangles[k].v2, objects[objIndex].triangles[k].v3, lightPos);
			}
						
            float p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z;
            if (!projectVertex(objects[objIndex].triangles[k].v1, &p1x, &p1y) || 
				!projectVertex(objects[objIndex].triangles[k].v2, &p2x, &p2y) ||
				!projectVertex(objects[objIndex].triangles[k].v3, &p3x, &p3y)) continue;
			
            drawEdge(p1x, p1y, p2x, p2y, pixels, shadedColor);
            drawEdge(p2x, p2y, p3x, p3y, pixels, shadedColor);
            drawEdge(p3x, p3y, p1x, p1y, pixels, shadedColor);
        }
    }
}

// Function to set the camera behind an object
void setCameraToObject(Object *obj, float fOffset, float uOffset, float rOffset) {
    // Update camera position
    cameraPos.x = obj->position[0] - (obj->forward[0] * fOffset) - (obj->up[0] * uOffset);
    cameraPos.y = obj->position[1] - (obj->forward[1] * fOffset) - (obj->up[1] * uOffset);
    cameraPos.z = obj->position[2] - (obj->forward[2] * fOffset) - (obj->up[2] * uOffset);

	if (!freeLook) {
	    // Update camera orientation
	    cameraOrientation = rotationMatrixToQuaternion(obj->forward, obj->up, obj->right);	
	    
	    Vec3 vecUp = {.x = obj->up[0],
					  .y = obj->up[1],
					  .z = obj->up[2]};
		rotateCamera(vecUp, M_PI); 
	}
}

void addPath(Object* object, PathDestination destination) {
    if (object == NULL) {
        printf("Error: object is NULL\n");
        return;
    }

    // If destinations is NULL, allocate memory instead of reallocating
    if (object->pathing.destinations == NULL) {
        object->pathing.destinations = (PathDestination*)malloc(sizeof(PathDestination));
        if (object->pathing.destinations == NULL) {
            printf("Error: Initial malloc failed\n");
            return;
        }
        object->pathing.numDestinations = 1;
        object->pathing.destinations[0] = destination;
        return;
    }

    // Increase destination count
    object->pathing.numDestinations++;

    // Reallocate memory for the new destination
    PathDestination* newDestinations = (PathDestination*)realloc(
        object->pathing.destinations,
        object->pathing.numDestinations * sizeof(PathDestination)
    );

    if (newDestinations == NULL) {
        printf("Error: Failed to allocate memory for path destinations\n");
        return;
    }

    // Assign the new memory block
    object->pathing.destinations = newDestinations;

    // Store the new destination in the last allocated slot
    object->pathing.destinations[object->pathing.numDestinations - 1] = destination;
}

void getPathVector(Object* object, float finalVector[3]) {
    // Safety check: Is object NULL?
    if (!object) {
        printf("Error: object is NULL\n");
        finalVector[0] = finalVector[1] = finalVector[2] = 0.0f;
        return;
    }

    PathParameters* pathing = &object->pathing;

    // Safety check: Does object have destinations?
    if (pathing->numDestinations == 0 || pathing->destinations == NULL) {
        printf("No valid destinations.\n");
        finalVector[0] = finalVector[1] = finalVector[2] = 0.0f;
        return;
    }

    float totalStrength = 0.0f;

    // Calculate total strength (ensure no bad memory access)
    for (uint32_t i = 0; i < pathing->numDestinations; i++) {
        if (&pathing->destinations[i] == NULL) continue;  // Avoid bad pointers
        totalStrength += pathing->destinations[i].strength;
    }

    if (totalStrength <= 0.0f) {
        printf("All destination strengths are zero or invalid.\n");
        finalVector[0] = finalVector[1] = finalVector[2] = 0.0f;
        return;
    }

    // Pick a destination based on weighted randomness
    float randomPick = ((float)rand() / RAND_MAX) * totalStrength;
    float cumulativeStrength = 0.0f;
    PathDestination* chosenDestination = NULL;

    for (uint32_t i = 0; i < pathing->numDestinations; i++) {
        cumulativeStrength += pathing->destinations[i].strength;
        if (randomPick <= cumulativeStrength) {
            chosenDestination = &pathing->destinations[i];
            break;
        }
    }

    // Fallback in case of failure (shouldn't happen, but extra safety)
    if (!chosenDestination) {
        printf("Warning: No destination chosen, defaulting to first one.\n");
        chosenDestination = &pathing->destinations[0];
    }

    // Compute movement vector
    finalVector[0] = chosenDestination->position[0] - object->position[0] + chosenDestination->velX;
    finalVector[1] = chosenDestination->position[1] - object->position[1] + chosenDestination->velY;
    finalVector[2] = chosenDestination->position[2] - object->position[2] + chosenDestination->velZ;

    // Compute vector magnitude
    float magnitude = sqrt(finalVector[0] * finalVector[0] + 
                           finalVector[1] * finalVector[1] + 
                           finalVector[2] * finalVector[2]);

    // Clamp vector length to prevent spinning issues
    if (magnitude > MAX_VECTOR_LENGTH) {
        finalVector[0] = (finalVector[0] / magnitude) * MAX_VECTOR_LENGTH;
        finalVector[1] = (finalVector[1] / magnitude) * MAX_VECTOR_LENGTH;
        finalVector[2] = (finalVector[2] / magnitude) * MAX_VECTOR_LENGTH;
    } 
    // Ensure the vector has a minimum length to prevent stalling
    else if (magnitude < MIN_VECTOR_LENGTH) {
        finalVector[0] = (finalVector[0] / magnitude) * MIN_VECTOR_LENGTH;
        finalVector[1] = (finalVector[1] / magnitude) * MIN_VECTOR_LENGTH;
        finalVector[2] = (finalVector[2] / magnitude) * MIN_VECTOR_LENGTH;
    }
}

void pathFindingVector(Object* object, float finalVector[3]) {
    if (!object) {
        printf("Error: object is NULL\n");
        finalVector[0] = finalVector[1] = finalVector[2] = 0.0f;
        return;
    }

    PathParameters* pathing = &object->pathing;

    // Safety check: Does object have destinations?
    if (pathing->numDestinations == 0 || pathing->destinations == NULL) {
        printf("No valid destinations.\n");
        finalVector[0] = finalVector[1] = finalVector[2] = 0.0f;
        return;
    }

    float totalStrength = 0.0f;

    // Calculate total strength (ensure no bad memory access)
    for (uint32_t i = 0; i < pathing->numDestinations; i++) {
        if (&pathing->destinations[i] == NULL) continue;  // Avoid bad pointers
        totalStrength += pathing->destinations[i].strength;
    }

    if (totalStrength <= 0.0f) {
        printf("All destination strengths are zero or invalid.\n");
        finalVector[0] = finalVector[1] = finalVector[2] = 0.0f;
        return;
    }

    // Pick a destination based on weighted randomness
    float randomPick = ((float)rand() / RAND_MAX) * totalStrength;
    float cumulativeStrength = 0.0f;
    PathDestination* chosenDestination = NULL;

    for (uint32_t i = 0; i < pathing->numDestinations; i++) {
        cumulativeStrength += pathing->destinations[i].strength;
        if (randomPick <= cumulativeStrength) {
            chosenDestination = &pathing->destinations[i];
            break;
        }
    }

    // Fallback in case of failure (shouldn't happen, but extra safety)
    if (!chosenDestination) {
        printf("Warning: No destination chosen, defaulting to first one.\n");
        chosenDestination = &pathing->destinations[0];
    }

    // Compute final movement vector
    finalVector[0] = chosenDestination->position[0] - object->position[0] + chosenDestination->velX;
    finalVector[1] = chosenDestination->position[1] - object->position[1] + chosenDestination->velY;
    finalVector[2] = chosenDestination->position[2] - object->position[2] + chosenDestination->velZ;
}

// Check for nearby objects and set the destination in the opposite direction
void avoidNearbyObjects(Object* objects, Object* currentObject, float* avoidanceStrength) {
    if (!objects || !currentObject) return;

    float avoidanceVector[3] = {0.0f, 0.0f, 0.0f};
    int nearbyObjectCount = 0;

    for (int i = 0; i < numObjects; i++) {
        if (&objects[i] == currentObject) continue; // Skip self
        
        float minDistance = objects[i].avoidanceRadius;

        // Check distance to other object
        float distance = fgetDistance3D(currentObject->position, objects[i].position);
        if (distance < minDistance) {
			if (*avoidanceStrength < minDistance) {
				*avoidanceStrength = minDistance;
			}
            // Accumulate the avoidance vector (opposite of the direction to the other object)
            avoidanceVector[0] += currentObject->position[0] - objects[i].position[0];
            avoidanceVector[1] += currentObject->position[1] - objects[i].position[1];
            avoidanceVector[2] += currentObject->position[2] - objects[i].position[2];
            nearbyObjectCount++;
        }
    }

    if (nearbyObjectCount > 0) {
        // Average the avoidance vector
        avoidanceVector[0] /= nearbyObjectCount;
        avoidanceVector[1] /= nearbyObjectCount;
        avoidanceVector[2] /= nearbyObjectCount;

        // Add random perturbation to avoid linear motion
        addRandomPerturbation(avoidanceVector, 0.3f);

        // Normalize to maintain direction
        fnormalize(avoidanceVector);

        // Set the destination to move away from nearby objects
        currentObject->pathing.destinations[0].position[0] = currentObject->position[0] + avoidanceVector[0] * 30.0f;
        currentObject->pathing.destinations[0].position[1] = currentObject->position[1] + avoidanceVector[1] * 30.0f;
        currentObject->pathing.destinations[0].position[2] = currentObject->position[2] + avoidanceVector[2] * 30.0f;

        currentObject->pathing.chasing = 0;
    }
}

void updateDestinationWithAvoidance(Object* objects, int j) {
    objects[j].pathing.chasing = 1;

	float avoidanceDistance;
    // Step 1: Avoid nearby objects (this updates the object's current destination)
    avoidNearbyObjects(objects, &objects[j], &avoidanceDistance);

    // Step 2: Get the avoidance position (after avoidNearbyObjects runs)
    float avoidancePos[3] = {
        objects[j].pathing.destinations[0].position[0],
        objects[j].pathing.destinations[0].position[1],
        objects[j].pathing.destinations[0].position[2]
    };

    // Step 3: Get the original target position (e.g., player or object[0])
    float targetPos[3] = {
        objects[0].position[0],
        objects[0].position[1],
        objects[0].position[2]
    };

    float targetWeight = 2.0f;
    float avoidanceWeight = 1.0f + avoidanceDistance;
    
    // Step 4: Normalize the weights
    float totalWeight = targetWeight + avoidanceWeight;
    float targetFactor = targetWeight / totalWeight;
    float avoidanceFactor = avoidanceWeight / totalWeight;

    // Step 5: Compute the weighted average position
    objects[j].pathing.destinations[0].position[0] = targetPos[0] * targetFactor + avoidancePos[0] * avoidanceFactor;
    objects[j].pathing.destinations[0].position[1] = targetPos[1] * targetFactor + avoidancePos[1] * avoidanceFactor;
    objects[j].pathing.destinations[0].position[2] = targetPos[2] * targetFactor + avoidancePos[2] * avoidanceFactor;
}

Vec3 doGravity(Object* obj1, Object* obj2) {
	Vec3 resultGrav = {.x = 0, .y = 0, .z = 0};
	
    float dist = fgetDistance3D(obj1->position, obj2->position);

    // Avoid division by zero or very small distances
    if (dist < 1e-6) return resultGrav;

    // Compute gravitational force magnitude
    float forceMagnitude = (G * obj1->mass * obj2->mass) / (dist * dist);

    // Compute acceleration (F = ma -> a = F / m)
    float acceleration = forceMagnitude / obj1->mass;

    // Compute direction vector
    float direction[3] = {
        obj2->position[0] - obj1->position[0],
        obj2->position[1] - obj1->position[1],
        obj2->position[2] - obj1->position[2]
    };
    fnormalize(direction);

    resultGrav.x = direction[0] * acceleration;
    resultGrav.y = direction[1] * acceleration;
    resultGrav.z = direction[2] * acceleration;
    return resultGrav;
}

void* processObjects(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    Object* objects = data->objects;
    
    for (int j = data->start; j < data->end; j++) {
        if (objects[j].id == 10) {
            updateDestinationWithAvoidance(objects, j);
            for (int i = 0; i < objects[j].pathing.numDestinations; i++) {                        
                float finalVector[3] = {0, 0, 0};
                getPathVector(&objects[j], finalVector);
                float objectCenter[3] = {0.0f, 0.0f, 0.0f};
                calculateObjectCenter(&objects[j], objectCenter);

                float distanceToTarget = fgetDistance3D(objects[j].position, objects[j].pathing.destinations[i].position);
                fnormalize(finalVector);
                finalVector[0] = fmod(finalVector[0], 2);
                finalVector[1] = fmod(finalVector[1], 2);
                finalVector[2] = fmod(finalVector[2], 2);
                turnTowardsPoint(&objects[j], objects[j].pathing.destinations[i].position);

                float damp = (distanceToTarget < (objects[j].parameters.minChaseDistance + objects[j].parameters.maxChaseDistance) / 10) 
                             ? 0.5f : 1.0f;
                damp *= MOVEMENT_DAMPENING;

                if (distanceToTarget >= objects[j].parameters.minChaseDistance &&
                    distanceToTarget <= objects[j].parameters.maxChaseDistance &&
                    objects[j].pathing.chasing) {
                    
                    objects[j].velX += objects[j].forward[0] * objects[j].parameters.forwardSpeed * damp;
                    objects[j].velY += objects[j].forward[1] * objects[j].parameters.forwardSpeed * damp;
                    objects[j].velZ += objects[j].forward[2] * objects[j].parameters.forwardSpeed * damp;
                } else if (!objects[j].pathing.chasing) {
                    objects[j].velX += objects[j].forward[0] * objects[j].parameters.forwardSpeed * damp;
                    objects[j].velY += objects[j].forward[1] * objects[j].parameters.forwardSpeed * damp;
                    objects[j].velZ += objects[j].forward[2] * objects[j].parameters.forwardSpeed * damp;
                }
            }
        } else {
			for (int i = 0; i < numObjects; i++) {
				if (&objects[j] == &objects[i] || objects[i].id == 10 || objects[i].id == 0 || objects[j].id == 10 || objects[j].id == 0) continue;
				Vec3 gravVelocity = doGravity(&objects[j], &objects[i]);
				objects[j].velX += gravVelocity.x;
				objects[j].velY += gravVelocity.y;
				objects[j].velZ += gravVelocity.z;
			}
			
		}

        moveObject(&objects[j], objects[j].velX, objects[j].velY, objects[j].velZ);
        if (!(objects[j].id == 1 || objects[j].id == 2)) {
			objects[j].velX *= objects[j].parameters.drag;
			objects[j].velY *= objects[j].parameters.drag;
			objects[j].velZ *= objects[j].parameters.drag;
		} else if (objects[j].id == 2) {
			//rotateObjectAroundAxis(&objects[j], objects[j].up, stars[objects[j].starIndex].spin);
		} else if (objects[j].id == 1) {
			//rotateObjectAroundAxis(&objects[j], objects[j].up, planets[objects[j].planetIndex].spin);
		}
		
        calculateObjectCenter(&objects[j], objects[j].position);

        //float time = SDL_GetTicks() * 0.0005f;
        //float r = (sin(time) + 1.0f) / 2.0f;
        //float g = (sin(time + (2.0f * M_PI / 3.0f)) + 1.0f) / 2.0f;
        //float b = (sin(time + (4.0f * M_PI / 3.0f)) + 1.0f) / 2.0f;
        //objects[j].color = ((int)(r * 255) << 16) | ((int)(g * 255) << 8) | (int)(b * 255);
    }
    return NULL;
}

void processObjectsMultithreaded(Object* objects) {
    pthread_t threads[NUM_THREADS];
    ThreadData threadData[NUM_THREADS];

    int objectsPerThread = numObjects / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        threadData[i].objects = objects;
        threadData[i].start = i * objectsPerThread;
        threadData[i].end = (i == NUM_THREADS - 1) ? numObjects : (i + 1) * objectsPerThread;
        pthread_create(&threads[i], NULL, processObjects, &threadData[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Definitely not elite (2025)",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL);

    if (!window) {
        printf("Window could not be created: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_Event event;
    
    // Get the performance frequency once, for later conversion
    uint64_t frequency = SDL_GetPerformanceFrequency();
    
    uint64_t lastTime = SDL_GetPerformanceCounter();
	uint64_t lastLogicTime = SDL_GetPerformanceCounter();
    uint64_t frameStart, frameEnd;
    
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    glewExperimental = GL_TRUE;
    glewInit();
    SDL_GL_SetSwapInterval(0);
    
    // Allocate pixel buffer (RGB format)
    unsigned char* pixels = (unsigned char*)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 3);
    if (!pixels) {
        printf("Failed to allocate pixel buffer\n");
        return -1;
    }
    unsigned char* pixels2 = (unsigned char*)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 3);
    if (!pixels2) {
        printf("Failed to allocate second pixel buffer\n");
        return -1;
    }
    
    addObject("cobra.bin", 1, 20000, 0, 0, 0xA900FF, 0);
    addObject("sphere.bin", 1000, 25000, 0, 0, 0x0000FF, 1);
    //addObject("theory.bin", 10000, 0, 0, 0, 0xFFFFFF, 0);

    //addPlanet((float[3]){100, 100, 100}, 0xFFFFFF, 10000, 0); 
    
    int amount = 1000;
    int size = (int)ceil(cbrt(amount)); // Find the cube root to arrange in 3D
	int index = 0; // Track object count
	
	for (int x = -size/2; x < size/2; x++) {
	    for (int y = -size/2; y < size/2; y++) {
	        for (int z = -size/2; z < size/2; z++) {
	            if (index >= amount) break; // Stop after placing
	
	            float posX = 20000 + x * 2.0f;
	            float posY = y * 2.0f;
	            float posZ = z * 2.0f;
	
	            // Add the object and set up its path
	            int objectID = addObject("viper.bin", 1, posX, posY, posZ, 0xFF0000, 10);
	            addPath(&objects[objectID], (PathDestination){.position = {0, 0, 0}, .velX = 0, .velY = 0, .velZ = 0, .strength = 1.0f});
	
	            //printf("Object %d placed at: [%.2f, %.2f, %.2f]\n", index, posX, posY, posZ);
	
	            index++;
	        }
	    }
	}
	
	//generateSkyboxStars((float[3]){0,0,0});
	
	settings.bumpscosity.value = 1;
    
    while (running) {
		// Just in case the frequency changes
	    frequency = SDL_GetPerformanceFrequency();
	    
		frameStart = SDL_GetPerformanceCounter();
		
		float frameSkyboxStart = SDL_GetTicks();
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }
        
		// Calculate elapsed time since last logic execution (in ticks)
        uint64_t elapsedTime = frameStart - lastLogicTime;
        float elapsedTimeInSeconds = (float)elapsedTime / frequency;
        float elapsedTimeInMs = elapsedTimeInSeconds * 1000.0f;
        
        
        uint64_t logicStart = SDL_GetPerformanceCounter();
        // Updates work on a 30 tps system
        if (elapsedTimeInMs >= 33.33f) {
			memset(pixels2, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
			handleInput(pixels);
            // Now that all the inputs have been handled, do the logic
	        if (!paused) {
				processObjectsMultithreaded(objects);
			}
        }
        
        if (firstPerson) {
			setCameraToObject(&objects[0], 0.3f, -0.5f, 0.0f);
		}
  		uint64_t logicEnd = SDL_GetPerformanceCounter();
                
        uint64_t renderStart = SDL_GetPerformanceCounter();
        camForward = rotateVecByQuat((Vec3){0, 0, -1}, cameraOrientation);
	    camRight = rotateVecByQuat((Vec3){1, 0, 0}, cameraOrientation);
	    camUp = rotateVecByQuat((Vec3){0, 1, 0}, cameraOrientation);
	    
        // Clear the pixel buffer (black background)
        memset(pixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
        
        if (!paused) {
			memcpy(pixels, pixels2, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
	        // Keep drawSkyboxStars out of the main render function, to make sure it's always first
			drawSkyboxStars(pixels);
			renderScene(pixels);
		} else {
			draw_pause_menu(pixels, &event);
		}
        uint64_t renderEnd = SDL_GetPerformanceCounter();
        
        uint64_t rasterStart = SDL_GetPerformanceCounter();
        // Get the current window size
		int windowWidth, windowHeight;
		SDL_GetWindowSize(window, &windowWidth, &windowHeight);
		
		// Set up an orthographic projection that matches the window size
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, windowWidth, 0, windowHeight, -10, 10);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		// Enable pixel zoom to scale the image to fit the window
		float zoomX = (float)windowWidth / (float)SCREEN_WIDTH;
		float zoomY = (float)windowHeight / (float)SCREEN_HEIGHT;
		glPixelZoom(zoomX, zoomY);
		
		// Set the raster position to the lower-left corner
		glRasterPos2i(0, 0);
		glDrawPixels(SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels);
		SDL_GL_SwapWindow(window);

        uint64_t rasterEnd = SDL_GetPerformanceCounter();
        
        static float fpsSum = 0.0f;
		static float frameTimeSum = 0.0f;
		static float logicTimeSum = 0.0f;
		static float renderTimeSum = 0.0f;
		static float rasterTimeSum = 0.0f;
		
		static int frameCount = 0;
		static uint64_t lastFpsUpdateTime = 0;
        
	    // Calculate frame time (in milliseconds)
	    uint64_t frameTime = frameStart - lastTime; // in ticks
	    float frameTimeInSeconds = (float)frameTime / frequency; // convert to seconds
	    float frameTimeInMs = frameTimeInSeconds * 1000.0f; // convert to milliseconds
	
	    float logicTime = ((uint64_t)(logicEnd - logicStart) / (double)frequency) * 1000.0;
	    float renderTime = ((uint64_t)(renderEnd - renderStart) / (double)frequency) * 1000.0;
	    float rasterTime = ((uint64_t)(rasterEnd - rasterStart) / (double)frequency) * 1000.0;
	
	    if (frameTime > 0) {
	        float actualFPS = 1000.0f / frameTimeInMs;
	
	        // Accumulate all metrics
	        fpsSum += actualFPS;
	        frameTimeSum += frameTimeInMs;
	        logicTimeSum += logicTime;
	        renderTimeSum += renderTime;
	        rasterTimeSum += rasterTime;
	        frameCount++;
	
	        uint64_t currentTime = frameStart; // Get current time in ticks
	        if (currentTime - lastFpsUpdateTime >= frequency) { // 1 second has passed
	            // Calculate averages
	            float avgFPS = fpsSum / frameCount;
	            float avgFrameTime = frameTimeSum / frameCount;
	            float avgLogicTime = logicTimeSum / frameCount;
	            float avgRenderTime = renderTimeSum / frameCount;
	            float avgRasterTime = rasterTimeSum / frameCount;
	
	            // Print averages
	            printf("AVG FPS: %-9.2f \tmspf: %-7.2f logic time: %-8.3f render time: %-8.3f raster time: %-8.3f (over %d frames)\n",
	                   avgFPS, avgFrameTime, avgLogicTime, avgRenderTime, avgRasterTime, frameCount);
	
	            // Reset counters
	            fpsSum = 0.0f;
	            frameTimeSum = 0.0f;
	            logicTimeSum = 0.0f;
	            renderTimeSum = 0.0f;
	            rasterTimeSum = 0.0f;
	            frameCount = 0;
	            lastFpsUpdateTime = currentTime;
	        }
	
	        // Print per-frame stats
	        //printf("FPS: %-9.2f \tmspf: %-7.2f logic time: %-8.3f render time: %-8.3f raster time: %-8.3f\n",
	        //       actualFPS, frameTimeInMs, logicTime, renderTime, rasterTime);
	    } else {
	        printf("Frametime less than 0?\n");
	    }

        // Update last time
        lastTime = frameStart;
    }
    
    freeObjects();
    free(pixels);
    free(pixels2);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
