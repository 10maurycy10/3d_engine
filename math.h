// Basic math functions

typedef struct Point2 {
	float x;
	float y;
} Point2;


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
