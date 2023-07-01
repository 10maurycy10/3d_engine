// Structures and Functions for working with 3d geometry
//
// z  y
// | /
// |/___x
//

#include <stdio.h>
#include "math.h"

// Each room conisists of a serize of walls, defined by a sererse of vertesez, located in 2d space, wich may be a portal connecting to other rooms
// A portal is denoted by a non negative portal idx, and has a 

struct WallVertex {
	struct Point2 location;
	int r, g, b;
	int texture;
	int portal_idx;
};

// A list of wall vertexis, length is the amount of points stored.
// This sould be heep allocated
// The wall vertecis must have a given order, so that from inside the room, the x cordinates are in acending order
struct Room {
	int floor_texture;
	int length;
	float z0;
	float z1;
	struct WallVertex walls[];
};

struct Map {
	Point2 starting_location;
	int starting_room;
	int length;
	struct Room* rooms[];
};

struct Camera {
	struct Point2 location;
	int room_idx;
	float z;
	float angle;
	float angle_sin;
	float angle_cos;
};



// Allocated a room with space for a certan amount of walls
struct Room* allocate_room(int length);

struct Map* allocate_map(int lenth);

// Allocate a test map
struct Map* new_test_map();

struct Map* load_map_from_file(FILE* file);

void free_room(struct Room*);

void free_map(struct Map*);

// Finds where a line segment from p0 to p1 intersects with the room geometry
// Returns the wall it intersects with if it does, -1 otherwise
// Optionaly, writes the point to point_of_collision if not NULL.
int room_collide(struct Room*, Point2 p0, Point2 p1, Point2* point_of_collision);
