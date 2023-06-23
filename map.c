// Structures and functions for working with 3d geometry
#include <stdlib.h>
#include <stdio.h>
#include "map.h"

// Allocated a room with capacity for n Rooms
struct Room* allocate_room(int length) {
	struct Room* room = malloc(sizeof(struct Room) + sizeof(struct WallVertex) * length);
	room->length = length;
	return room;
}

struct Map* allocate_map(int length) {
	struct Map* map = malloc(sizeof(struct Map) + sizeof(struct Room*) * length);
	map->length = length;
	return map;
}

void free_room(struct Room* room) {
	free(room);
}

void free_map(struct Map* map) {
	for (int i = 0; i < map->length; i++) {
		printf("Room Freed\n");
		free_room(map->rooms[i]);
	}
	printf("Map Freed\n");
	free(map);
}

struct Map* new_test_map() {
	struct Room* room1 = allocate_room(4);
	room1->walls[0].location.x = 1;
	room1->walls[0].location.y = 2;

	room1->walls[1].location.x = 1;
	room1->walls[1].location.y = -2;
	
	room1->walls[2].location.x = -1;
	room1->walls[2].location.y = -2;
	
	room1->walls[3].location.x = -1;
	room1->walls[3].location.y = 2;

	struct Map* map = allocate_map(1);
	map->rooms[0] = room1;
	return map;
}

struct Point2 point2_mul_scaler(struct Point2 l, float scale) {
	struct Point2 o = {.x = l.x * scale, .y = l.y * scale};
	return o;
}

struct Point2 point2_div_scaler(struct Point2 l, float scale) {
	struct Point2 o = {.x = l.x * scale, .y = l.y * scale};
	return o;
}

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
