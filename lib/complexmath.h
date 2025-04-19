#ifndef COMPLEX_MATH_H
#define COMPLEX_MATH_H

/* IMPORTANT
 * When using this, make sure every returned value is freed up from ram 
 *   after it is no longer useful.
 * 
 * All units used are assumd to be SI, for accuracy. 
 * Also, everything that would, only has x and y, no z, w, etc
 * 
 * All comments about what the functions do is in the c code.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_HYPEROPERATION_DEPTH 100000

typedef struct {
	// direction is 0 to 360 in degrees
	// amplitude is simply a number above 0 (negative vector size just doesn't work)
	float direction, amplitude;
} Vector;

typedef struct {
	float amplitudeX, amplitudeY;
} DecomposedVector;

typedef struct {
	float x, y;
} Vec2;

typedef struct {
	float x, y, z;
} Vec3;

typedef struct {
	float x, y, z, w;
} Vec4;

typedef struct {
	float accelX, accelY; // Acceleration of object in both directions
	float velocityX, velocityY; // Velocity of object in both directions, assumed m/s
	float mass; // Mass of object, assumed kg
	float friction; // Friction coeffecient, or 'μ' (just a number, no unit)
	bool immobile; 
} ObjectPhysics;

// general math functions

uint64_t hyperoperation(int n, uint64_t a, uint64_t b, int depth);

void setVec2(Vec2 *Vec2, int x, int y);
void changeVec2(Vec2 *Vec2, int x, int y);

// more specific geometric stuff

Vector* combineVectors(Vector *vect1, Vector *vect2);
Vec2* rotateBox(Vec2 *corners[], float *degrees);

Vec2* rotateQuad(Vec2 *corners[], float *degrees);
Vec2 Vec2FromAngle(int angle);

#endif