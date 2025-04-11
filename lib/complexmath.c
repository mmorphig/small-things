#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "complexmath.h"

// This doesn't all have to be 'complex'
/* As the names imply, setPoint sets the x and y of the point to the inputs
 * changePoint add the new values to the x and y of the point
 */
void setPoint(Point *point, int x, int y) {
	point->x = x;
	point->y = y;
}

void changePoint(Point *point, int x, int y) {
	point->x += x;
	point->y += y;
}

// Vector math

Vector* combineVectors(Vector *vect1, Vector *vect2) {

	if (vect1 == NULL || vect2 == NULL) {
		// If either vectors are null, show the error
		printf("NULL pointer found in vector combination\n");
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

// Calculate new points for rotation of a quadrilaterial
// corners[] is an array of pointers and MUST be used as such (compiler might not like otherwise)
Point* rotateQuad(Point *corners[], float *degrees) {

	if (corners[0] == NULL || corners[1] == NULL || corners[2] == NULL || corners[3] == NULL || degrees == NULL) {
		// If any value is null, show the error
		printf("NULL pointer found in quadrilaterial rotation");
		return NULL;
	}

	// Convert faceDirection to radians for rotation calculations
	double radians = (*degrees * M_PI) / 180.0;

	// Find the center of the box (average of the corners)
	float centerX = (corners[0]->x + corners[1]->x + corners[2]->x + corners[3]->x) / 4;
	float centerY = (corners[0]->y + corners[1]->y + corners[2]->y + corners[3]->y) / 4;

	// Function to rotate a point around the center of the box
	void rotatePoint(Point *p, Point *result) {
		result->x = (float)(cos(radians) * (p->x - centerX) - sin(radians) * (p->y - centerY) + centerX);
		result->y = (float)(sin(radians) * (p->x - centerX) + cos(radians) * (p->y - centerY) + centerY);
	}

	// Allocate memory for the new points
	Point* newCorners = malloc(4 * sizeof(Point));
	if (newCorners == NULL) {
		// Memory allocation failed
		printf("Memory allocation failed in quadrilaterial rotation\n");
		return NULL;
	}

	// Rotate all the corners of the box and store them in newCorners
	rotatePoint(corners[0], &newCorners[0]);
	rotatePoint(corners[1], &newCorners[1]);
	rotatePoint(corners[2], &newCorners[2]);
	rotatePoint(corners[3], &newCorners[3]);

	return newCorners; // Caller must free this memory
}

// Trigonometry stuff maybe don't use these and just use 

// Unit circle
// Probably useless
Point PointFromAngle(int angle) {
	Point result = {.x = cos(angle), .y = sin(angle)};  
	return result;
}
