#include "math.h"
#include <math.h>

////////////////////////
// Vector opertations //
////////////////////////

struct Point2 point2_mul_scaler(struct Point2 l, float scale) {
	struct Point2 o = {.x = l.x * scale, .y = l.y * scale};
	return o;
}

struct Point2 point2_div_scaler(struct Point2 l, float scale) {
	struct Point2 o = {.x = l.x * scale, .y = l.y * scale};
	return o;
}

/////////////////////////////
// Geometry math functions //
/////////////////////////////

// Math stolen from https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection#Given_two_points_on_each_line
struct Point2 intersect_line_segments(Point2 a0, Point2 a1, Point2 b0, Point2 b1) {
        // The end point of the line segments if translated so that the start is at 0, 0
	Point2 v0 = {.x = a1.x - a0.x, .y = a1.y - a0.y};
	Point2 v1 = {.x = b1.x - b0.x, .y = b1.y - b0.y};

	float d = (-v1.x * v0.y + v0.x * v1.y);
	
	// Lines are collinear or paralel, or very close to it.
	if (fabsf(d) < 0.00001)
		return (Point2) {NAN, NAN};

	// Find how far along each line segment the intersection is.
	float t =  (-v0.y * (a0.x - b0.x) + v0.x * (a0.y - b0.y)) / d;
	float u =  ( v1.x * (a0.y - b0.y) - v1.y * (a0.x - b0.x)) / d;

	// If it is withing both segments, return it.
	if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                return (Point2) {
                        a0.x + (u * v0.x),
                        a0.y + (u * v0.y)
                };
        } else {
                return (Point2) {NAN, NAN};
        }
}

// Math stolen from https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection#Given_two_points_on_each_line
struct Point2 intersect_lines(Point2 a0, Point2 a1, Point2 b0, Point2 b1) {
	// The end point of the line segments if translated so that the start is at 0, 0
	Point2 v0 = {.x = a1.x - a0.x, .y = a1.y - a0.y};
	Point2 v1 = {.x = b1.x - b0.x, .y = b1.y - b0.y};

	float d = (-v1.x * v0.y + v0.x * v1.y);
	
	// Lines are collinear or paralel, or very close to it.
	if (fabsf(d) < 0.00001)
		return (Point2) {NAN, NAN};

	// Find how far along each line segment the intersection is.
	float u =  ( v1.x * (a0.y - b0.y) - v1.y * (a0.x - b0.x)) / d;

	return (Point2) {
		a0.x + (u * v0.x),
		a0.y + (u * v0.y)
	};
}

float lerp(float x0, float x1, float d) {
	return (x1 * d) + (x0 * (1-d));
}
