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
	int portal_idx;
};

// A list of wall vertexis, length is the amount of points stored.
// This sould be heep allocated
// The wall vertecis must have a given order, so that from inside the room, the x cordinates are in acending order
struct Room {
	int length;
	struct WallVertex walls[];
};

struct Map {
	int length;
	struct Room* rooms[];
};

struct Location {
	int roomid;
	float x;
	float y;
};

struct Camera {
	struct Location location;
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

