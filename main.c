#include "map.h"
#include <assert.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720
// How far from the center of the viewing plain (1 unit away from camera) should the screen be?
#define FOV 1

// Graphics initalizeation
SDL_Window* window_setup() {
	int windowFlags = 0;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_Window* window = SDL_CreateWindow("3d", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, windowFlags);

	if (!window) {
		printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
		exit(1);
	}

	return window;
}

void do_input(struct Camera* camera) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				exit(0);
				break;
			default:
				break;
		}
	}

	uint8_t* keyboard = SDL_GetKeyboardState(NULL);

	if (keyboard[SDL_SCANCODE_Q]) {
		camera->angle -= 0.01;
	}
	if (keyboard[SDL_SCANCODE_E]) {
		camera->angle += 0.01;
	}

	// TODO Move relitive to camera rotation
	if (keyboard[SDL_SCANCODE_W]) {
		camera->location.y += 0.05;
	}
	if (keyboard[SDL_SCANCODE_S]) {
		camera->location.y -= 0.05;
	}
	if (keyboard[SDL_SCANCODE_A]) {
		camera->location.x -= 0.05;
	}
	if (keyboard[SDL_SCANCODE_D]) {
		camera->location.x += 0.05;
	}
}

/////////////////////////
// Graphics Primitives //
/////////////////////////

void set_pixel(SDL_Surface* canvas, int x, int y, int r, int g, int b) {
	((uint8_t*)canvas->pixels)[(x*4 + y * canvas->pitch)+0] = r;
	((uint8_t*)canvas->pixels)[(x*4 + y * canvas->pitch)+1] = g;
	((uint8_t*)canvas->pixels)[(x*4 + y * canvas->pitch)+2] = b;
	((uint8_t*)canvas->pixels)[(x*4 + y * canvas->pitch)+3] = 255;
}

void vline(SDL_Surface* canvas, int x, int y0, int y1, int r, int g, int b) {
	for (int y = y0; y < y1; y++) {
		set_pixel(canvas, x, y, r, g, b);
	}
}

////////////////////////////////
// Cordinate space convertion //
////////////////////////////////

// Convert a point from world space to camera based cordinates
// In this system, the camera is at 0,0 and facing the +y direction.
Point2 world_to_camera_space(struct Camera* camera, Point2 world) {
	Point2 translated = {.x = world.x - camera->location.x, .y = world.y - camera->location.y };

	return (Point2) {
		.x = translated.x * camera->angle_cos - translated.y * camera->angle_sin,
		.y = translated.x * camera->angle_sin + translated.y * camera->angle_cos
	};
}

// Convert a Point in camera space to normalized screen space.
Point2 camera_to_screen_space(Point2 cameraspace, float z, float fov) {
	return (Point2) {
		.x = cameraspace.x / cameraspace.y / fov,
		.y = z / cameraspace.y / fov,
	};
}

// Scale and translate the screen cordinates to pixel cordinates for SDL.
Point2 normalized_screen_to_pixel(Point2 world, float screenh, float screenw) {
	return (Point2) {
		.x = (world.x + 1) * (screenw / 2),
		.y = ((-world.y) + 1) * (screenh / 2),
	};
}

// All in one function to go from camera space to pixels
Point2 camera_to_pixel_space(Point2 camera, float z, float screenh, float screenw, float fov) {
	return normalized_screen_to_pixel(camera_to_screen_space(camera,z,fov), screenh, screenw);
}

// Returns false if the wall is fully outside, true otherwize
int clip_to_frustum(Point2* w0, Point2* w1, float fov) {
	float near_plane = 0.1;

	// Clip to the near plane
	
	// If both are behind the near plane, reject the wall
	if (w0 -> y < near_plane && w1 -> y < near_plane) return 0;

	// If only one is, move it 
	if (w0->y < near_plane) {
		*w0 = intersect_lines((Point2) {1, near_plane}, (Point2) {-1, near_plane}, *w0, *w1);
	}
	if (w1->y < near_plane) {
		*w1 = intersect_lines((Point2) {1, near_plane}, (Point2) {-1, near_plane}, *w0, *w1);
	}
	
	// Clip by angle
	float angle0 = w0->x / w0->y;
	float angle1 = w1->x / w1->y;
	
	// If both endpoints are out of view, on the same side, reject the wall
	if (angle0 > fov && angle1 > fov) return 0;
	if (angle0 < -fov && angle1 < -fov) return 0;
	
	// Otherwize, move the points inside of the view.
	if (angle1 > fov) *w1 = intersect_lines((Point2) {0, 0}, (Point2) {fov, 1}, *w0, *w1);
	if (angle1 < -fov) *w1 = intersect_lines((Point2) {0, 0}, (Point2) {-fov, 1}, *w0, *w1);
	
	if (angle0 > fov) *w0 = intersect_lines((Point2) {0, 0}, (Point2) {fov, 1}, *w0, *w1);
	if (angle0 < -fov) *w0 = intersect_lines((Point2) {0, 0}, (Point2) {-fov, 1}, *w0, *w1);


	return 1;
}

void render_room(SDL_Surface* canvas, struct Camera* camera, int roomid, struct Map* map) {
	// Calculate and store sin and cos of camera angle, this is required for rotation
	camera->angle_cos = cos(camera->angle);
	camera->angle_sin = sin(camera->angle);
	
	struct Room* room = map->rooms[roomid];
	// For every wall in the players room...
	for (int wallid = 0; wallid < room->length; wallid++) {
		// Find the start and end vertex.
		// The wall parameters sould be stored in the first one
		struct WallVertex wallstart = room->walls[wallid];
		struct WallVertex wallend;
		if (wallid == room->length - 1) {
			wallend = room->walls[0];
		} else {
			wallend = room->walls[wallid + 1];
		}

		Point2 cspace0 = world_to_camera_space(camera, wallstart.location);
		Point2 cspace1 = world_to_camera_space(camera, wallend.location);
		
		// Wall fully behind camera, do not render
		if (cspace0.y <= 0 && cspace1.y <= 0) continue;
		
		// Clip to fustrum, if fully outside, dont render.	
		if (!clip_to_frustum(&cspace0, &cspace1, FOV)) continue;
		
		// Wall verteces are in acending x order, as seen from the inside of the room.
		// If this is not the case, we are looking at the backside of the wall, and should skip rendering it
		if (cspace1.x/cspace1.y <= cspace0.x/cspace0.y) continue;
		
		// Map to screen space	
		struct Point2 wall_corner_0_u = camera_to_pixel_space(cspace0, 0.5, SCREEN_HEIGHT, SCREEN_WIDTH, FOV);
		struct Point2 wall_corner_0_l = camera_to_pixel_space(cspace0, -0.5, SCREEN_HEIGHT, SCREEN_WIDTH, FOV);
		struct Point2 wall_corner_1_u = camera_to_pixel_space(cspace1, 0.5, SCREEN_HEIGHT, SCREEN_WIDTH, FOV);
		struct Point2 wall_corner_1_l = camera_to_pixel_space(cspace1, -0.5, SCREEN_HEIGHT, SCREEN_WIDTH, FOV);

		// Draw filled trapiziod defined by projected points
		// There might be a faster drawing algoritm than this
		int lines = wall_corner_1_u.x - wall_corner_0_u.x;
		for (int line = 0; line < lines; line++) {
			int pixelx = wall_corner_0_u.x + line;
			float distance_drawn = (float)line / (float)lines;
			int y0 = lerp(wall_corner_0_u.y, wall_corner_1_u.y, distance_drawn);
			int y1 = lerp(wall_corner_0_l.y, wall_corner_1_l.y, distance_drawn);
			y0 = MAX(y0, 0);
			y1 = MIN(y1, SCREEN_HEIGHT);
			assert(y0 <= y1);
			vline(canvas, pixelx, y0, y1, 255, 255, 255);
		}
	}	
}

int main() {
	// Open a window
	SDL_Window* window = window_setup();
	assert(window);
	SDL_Surface* window_surface = SDL_GetWindowSurface(window);
	assert(window_surface);

	// Create a buffer for drawing.
	// Alpha channel will serve as a flag for storing if a pixel has been drawn.
	SDL_Surface *canvas = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_RGBA8888);
	assert(canvas);

	struct Map* map = new_test_map();
	struct Camera camera = {.location = {.roomid = 0, .x = 0, .y = -4}};

	// Sanity check, make sure the player has a valid room
	assert(map->length > camera.location.roomid);

	while (1) {
		// Handle inputs
		do_input(&camera);
	
		// Prepare for drwwing
		SDL_LockSurface(canvas);
		
		// Fill with hot pink to make unrendered areas obvios. 
		SDL_FillRect(canvas, NULL, 0xFF00FFFF);

		render_room(canvas, &camera, camera.location.roomid, map);

		// Done drawing
		SDL_UnlockSurface(canvas);

		// Copy the buffer to the window
		SDL_BlitSurface(canvas, NULL, window_surface, NULL);
		SDL_UpdateWindowSurface(window);
	}

	free_map(map);
	return 0;
}
