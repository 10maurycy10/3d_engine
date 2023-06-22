// Structures and Functions for working with 3d geometry
//
// z  y
// | /
// |/___x
//

#include <math.h>

// Trivial Structures for working with multidementional data
typedef struct Point2 {
	float x;
	float y;
} Point2;

// Each room conisists of a serize of walls, defined by a sererse of vertesez, located in 2d space, wich may be a portal connecting to other rooms
// A portal is denoted by a non negative portal idx, and has a 

struct WallVertex {
	struct Point2 location;
	int portal_idx;
//	float portal_start_z;
//	float portak_end_z;
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

void free_room(struct Room*);

void free_map(struct Map*);

// Math functions

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)>(b)?(b):(a))

struct Point2 point2_mul_scaler(struct Point2, float scale);
struct Point2 point2_div_scaler(struct Point2, float scale);

// Find the intersection of two line segments, returns NAN, NAN if they do not intersect
struct Point2 intersect_line_segments(Point2 a0, Point2 a1, Point2 b0, Point2 b1);

// Find the intersection point of 2 lines.
struct Point2 intersect_lines(Point2 a0, Point2 a1, Point2 b0, Point2 b1);

// Linear interpolation
float lerp(float x0, float x1, float d);
