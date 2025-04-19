#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "complexmath.h"

// This doesn't all have to be 'complex'
/* As the names imply, setVec2 sets the x and y of the Vec2 to the inputs
 * changeVec2 add the new values to the x and y of the Vec2
 */
void setVec2(Vec2 *Vec2, int x, int y) {
	Vec2->x = x;
	Vec2->y = y;
}

void changeVec2(Vec2 *Vec2, int x, int y) {
	Vec2->x += x;
	Vec2->y += y;
}

uint64_t hyperoperation(int n, uint64_t a, uint64_t b, int depth) {
    if (depth > MAX_HYPEROPERATION_DEPTH) {
        printf("Exceeded maximum recursion depth!\n");
        return 0;
    }

    if (n == 0) return b + 1;
    if (n == 1) return a + b;
    if (n == 2) return a * b;
    if (n == 3) {
        uint64_t result = 1;
        for (uint64_t i = 0; i < b; i++) result *= a;
        return result;
    }

    // For higher-order operations: H(n, a, b) = H(n - 1, a, H(n, a, b - 1))
    if (b == 0) return (n == 4) ? 1 : 0;

    return hyperoperation(n - 1, a, hyperoperation(n, a, b - 1, depth + 1), depth + 1);
}

// Vector math

Vector* combineVectors(Vector *vect1, Vector *vect2) {

	if (vect1 == NULL || vect2 == NULL) {
		// If either vectors are null, show the error
		printf("NULL Vec2er found in vector combination\n");
		return NULL;
	}

	// Convert degrees to radians because C uses rad and not deg
	// (doing this uses slightly more ram (maybe), but makes it so the calculation is only done once per vector)
	float vect1Rad = (vect1->direction * M_PI) / 180.0;
	float vect2Rad = (vect2->direction * M_PI) / 180.0;

	// Convert vect1 to Cartesian coordinates
	float x1 = vect1->amplitude * cos(vect1Rad);
	float y1 = vect1->amplitude * sin(vect1Rad);

	// Convert vect2 to Cartesian coordinates
	float x2 = vect2->amplitude * cos(vect2Rad);
	float y2 = vect2->amplitude * sin(vect2Rad);

	// Sum the X and Y components
	float resultX = x1 + x2;
	float resultY = y1 + y2;

	// Calculate the resultant vector's amplitude (magnitude)
	float newAmplitude = sqrt((resultX * resultX) + (resultY * resultY));

	// Calculate the resultant vector's direction (angle in degrees)
	float newDirection = atan2(resultY, resultX) * (180.0 / M_PI);
	// Ensure the direction is within the range 0 to 360 degrees
	if (newDirection < 0.0f || newDirection == -0.0f) {
	  newDirection += 360.0f;
  }

	Vector* newVector;

	newVector->direction = newDirection;
	newVector->amplitude = sqrt((resultX * resultX) + (resultY * resultY));

	return newVector; // Caller must free this memory
}


DecomposedVector decomposeVector(Vector* vect) {
	// Convert degrees to radians because C uses rad and not deg
	// (doing this uses slightly more ram (maybe), but makes it so the calculation is only done once per vector)
	float vectRad = (vect->direction * M_PI) / 180.0;

	// Convert vector
	float x1 = vect->amplitude * cos(vectRad);
	float y1 = vect->amplitude * sin(vectRad);
	
	DecomposedVector result = {.amplitudeX= x1, .amplitudeY= y1};
	return result;
}

// Misc

// Calculate new Vec2s for rotation of a quadrilaterial
// corners[] is an array of Vec2ers and MUST be used as such (compiler might not like otherwise)
Vec2* rotateQuad(Vec2 *corners[], float *degrees) {

	if (corners[0] == NULL || corners[1] == NULL || corners[2] == NULL || corners[3] == NULL || degrees == NULL) {
		// If any value is null, show the error
		printf("NULL Vec2er found in quadrilaterial rotation");
		return NULL;
	}

	// Convert faceDirection to radians for rotation calculations
	double radians = (*degrees * M_PI) / 180.0;

	// Find the center of the box (average of the corners)
	float centerX = (corners[0]->x + corners[1]->x + corners[2]->x + corners[3]->x) / 4;
	float centerY = (corners[0]->y + corners[1]->y + corners[2]->y + corners[3]->y) / 4;

	// Function to rotate a Vec2 around the center of the box
	void rotateVec2(Vec2 *p, Vec2 *result) {
		result->x = (float)(cos(radians) * (p->x - centerX) - sin(radians) * (p->y - centerY) + centerX);
		result->y = (float)(sin(radians) * (p->x - centerX) + cos(radians) * (p->y - centerY) + centerY);
	}

	// Allocate memory for the new Vec2s
	Vec2* newCorners = malloc(4 * sizeof(Vec2));
	if (newCorners == NULL) {
		// Memory allocation failed
		printf("Memory allocation failed in quadrilaterial rotation\n");
		return NULL;
	}

	// Rotate all the corners of the box and store them in newCorners
	rotateVec2(corners[0], &newCorners[0]);
	rotateVec2(corners[1], &newCorners[1]);
	rotateVec2(corners[2], &newCorners[2]);
	rotateVec2(corners[3], &newCorners[3]);

	return newCorners; // Caller must free this memory
}

// Trigonometry stuff maybe don't use these and just use 

// Unit circle
// Probably useless
Vec2 Vec2FromAngle(int angle) {
	Vec2 result = {.x = cos(angle), .y = sin(angle)};  
	return result;
}
