#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

/* IMPORTANT
 * When using this, make sure every returned value is freed up from ram 
 *   after it is no longer useful.
 * I made this with SDL2 in mind, but have made it completely independent
 *   from it so it can be used in things that don't use SDL2
 * 
 * All units used are assumd to be SI, for accuracy. 
 * Also, everything that would, only has x and y, no z, w, etc
 * 
 * All comments about what the functions do is in the c code.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	// direction is 0 to 360 in degrees
	// amplitude is simply a number above 0 (negative vector size just doesn't work)
	float direction, amplitude;
} Vector;

typedef struct {
	float amplitudeX, amplitudeY;
} DecomposedVector;

typedef struct {
	// x and y could be as int, but for mathematical precision, it's better to use float (or maybe double, but I'm not doing that)
	float x, y;
} Point;

typedef struct {
	float accelX, accelY; // Acceleration of object in both directions
	float velocityX, velocityY; // Velocity of object in both directions, assumed m/s
	float mass; // Mass of object, assumed kg
	float friction; // Friction coeffecient, or 'μ' (just a number, no unit)
	bool immobile;
} ObjectPhysics;

void setPoint(Point *point, int x, int y);
void changePoint(Point *point, int x, int y);

Vector* combineVectors(Vector *vect1, Vector *vect2);
Point* rotateBox(Point *corners[], float *degrees);

#endif