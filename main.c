#include "map.h"
#include <assert.h>
#include <SDL2/SDL.h>

// How far from the center of the viewing plain (1 unit away from camera) should the screen be?
#define FOV .6

/////////////////////////
// Graphics Primitives //
/////////////////////////

void vline(SDL_Renderer* canvas, int x, int y0, int y1, int r, int g, int b) {
	SDL_SetRenderDrawColor(canvas, r, g, b, 255);
	SDL_RenderDrawLine(canvas, x, y0, x, y1);
}

//////////////////////////////////////////////////
// Cordinate space convertion                   //
// This handles projection and camera positions //
//////////////////////////////////////////////////

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

/////////////
// Cliping //
/////////////

// This computes how much of a wall is visable, storing the start and end in w1
// Returns false if the wall is fully outside, true otherwize
int clip_to_frustum(Point2* w0, Point2* w1, float fov) {
	float near_plane = 0.0;

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

//////////////////////////////////////////////////////////////////////////////////
// X Bound                                                                      //
// this allows the renderer to avoid drawing part of a room that is not visable //
//////////////////////////////////////////////////////////////////////////////////

typedef struct Bound {
	int x0;
	int x1;
} Bound;

Bound whole_screen(int w) {
	return (Bound) {
		.x0 = 0,
		.x1 = w,
	};
}

//////////////
// Renderer //
////////////// 


// This is the main rendering function, it renders a single room (section of convex geometry).
// No z-sorting or deapth buffering is required as the geometry rendered is sticktly convex 
// This function will recurse to draw connected rooms.
// bound, y_min and y_max are used to avoid overdraw.
// Bound is the x bounds for the render, 
// y_min and y_max are used to store how much space is unrendered at a given z cordinate
// A single maximum or minimum value cant be used here as portals can oclude eachother in complex ways
void render_room(SDL_Renderer* canvas, struct Camera* camera, int roomid, struct Map* map, struct Bound bound, int w, int h, int y_min[], int y_max[]) {
	// Calculate and store sin and cos of camera angle, this is required for rotation
	camera->angle_cos = cos(camera->angle);
	camera->angle_sin = sin(camera->angle);

	struct Room* room = map->rooms[roomid];

	int floor_r = (roomid * 32) % 256;
	int floor_g = (roomid * 64 + 128) % 256;
	int floor_b = (roomid * 128) % 256;

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
		
		// Project to screen space	
		struct Point2 wall_corner_0_u = camera_to_pixel_space(cspace0, room->z1 - camera->z, h, w, FOV);
		struct Point2 wall_corner_0_l = camera_to_pixel_space(cspace0, room->z0 - camera->z, h, w, FOV);
		struct Point2 wall_corner_1_u = camera_to_pixel_space(cspace1, room->z1 - camera->z, h, w, FOV);
		struct Point2 wall_corner_1_l = camera_to_pixel_space(cspace1, room->z0 - camera->z, h, w, FOV);

		// Draw filled trapiziod defined by projected points, and fill the area above and below black
		// There might be a faster drawing algoritm than this
		int lines = wall_corner_1_u.x - wall_corner_0_u.x;
		if (lines == 0) continue;

		// Take bounds into account
		int startline = 0;
		int endline = lines;
		if (wall_corner_0_u.x < bound.x0) startline = bound.x0 - wall_corner_0_u.x;
		if (wall_corner_1_u.x > bound.x1) endline = bound.x1 - wall_corner_0_u.x;

		// Ingore non visable walls
		if (startline > lines || endline < 0) continue;

		for (int line = startline; line <= endline; line++) {
			int pixelx = wall_corner_0_u.x + line;
			float distance_drawn = (float)line / (float)lines;
			int y0 = lerp(wall_corner_0_u.y, wall_corner_1_u.y, distance_drawn);
			int y1 = lerp(wall_corner_0_l.y, wall_corner_1_l.y, distance_drawn);
			// Avoid drawing ontop of walls in previos sectors
			y0 = MAX(y0, y_min[pixelx]);
			y1 = MIN(y1, y_max[pixelx]);
			
			// Draw floor
			vline(canvas, pixelx, y_min[pixelx], y0, floor_r, floor_g, floor_b);
			// If this is a normal wall, draw it in
			if (wallstart.portal_idx == -1) {
				vline(canvas, pixelx, y0, y1, wallstart.r, wallstart.g, wallstart.b);
			}
			// Draw the cleiling
			vline(canvas, pixelx, y1, y_max[pixelx], floor_r, floor_g, floor_b);

			y_min[pixelx] = y0;
			y_max[pixelx] = y1;
		}
		if (wallstart.portal_idx != -1){
		 	// In the case of a portal, compute to intersection of the current bound and the portal.
			struct Bound portalbound = {
				.x0 = MAX(bound.x0, wall_corner_0_u.x),
				.x1 = MIN(bound.x1, wall_corner_1_u.x),
			};
			// Ensure the bound is not empty
			if (portalbound.x0 >= portalbound.x1) continue;
			// Recurse to draw room beond portal
			render_room(canvas, camera, wallstart.portal_idx, map, portalbound, w, h, y_min, y_max);
		}
	}	
}

/////////////
// UI code //
/////////////

SDL_Window* window_setup() {
	int windowFlags = SDL_WINDOW_RESIZABLE;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_Window* window = SDL_CreateWindow("3d", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1200, 720, windowFlags);

	if (!window) {
		printf("Failed to open window: %s\n", SDL_GetError());
		exit(1);
	}

	return window;
}

void do_input(struct Map* map, struct Camera* camera) {

	camera->z = map->rooms[camera->room_idx]->z0 + 1;

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

	// Handle rotation inputs
	if (keyboard[SDL_SCANCODE_Q]) {
		camera->angle -= 0.05;
	}
	if (keyboard[SDL_SCANCODE_E]) {
		camera->angle += 0.05;
	}

	// Vector to store movement intent
	// The camera is facing y+.
	Point2 translation = {0, 0};
	if (keyboard[SDL_SCANCODE_W]) {
		translation.y += 0.1;
	}
	if (keyboard[SDL_SCANCODE_S]) {
		translation.y -= 0.1;
	}
	if (keyboard[SDL_SCANCODE_A]) {
		translation.x -= 0.1;
	}
	if (keyboard[SDL_SCANCODE_D]) {
		translation.x += 0.1;
	}

	Point2 old_location = camera->location;
	
	// Rotate intent vector by *negative* camera rotation.
	float angle = -camera->angle;
	camera->location.x += translation.x * cos(angle) - translation.y * sin(angle);
	
	// Check for collisions
	int collision = room_collide(map->rooms[camera->room_idx], camera->location, old_location, NULL);
	if (collision != -1) {
		struct WallVertex wall = map->rooms[camera->room_idx]->walls[collision];
		if (wall.portal_idx != -1) {
			// Portal, allow movement, but update roomidx
			camera->room_idx = wall.portal_idx;
		} else {
			// Wall, ignore movement
			camera->location = old_location;
		}
	}
	
	old_location = camera->location;
	camera->location.y += translation.x * sin(angle) + translation.y * cos(angle);
	
	// Check for collisions
	collision = room_collide(map->rooms[camera->room_idx], camera->location, old_location, NULL);
	if (collision != -1) {
		struct WallVertex wall = map->rooms[camera->room_idx]->walls[collision];
		if (wall.portal_idx != -1) {
			// Portal, allow movement, but update roomidx
			camera->room_idx = wall.portal_idx;
		} else {
			// Wall, ignore movement
			camera->location = old_location;
		}
	}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Usage: %s [mapfile]\n", argv[0]);
		return 1;
	}

	char* mapfile = argv[1];

	// Open a window
	SDL_Window* window = window_setup();
	assert(window);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC); 
	assert(renderer);

	struct Map* map = load_map_from_file(fopen(mapfile, "r"));
	struct Camera camera = {.location = map->starting_location, .room_idx=map->starting_room, .z=0};


	while (1) {
		// Handle inputs
		do_input(map, &camera);
			
		// Sanity check, make sure the player has a valid room
		assert(map->length > camera.room_idx);

		// Fill viewport with hot pink to make unrendered areas easly visiable
		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);

		int h, w;
		SDL_GetRendererOutputSize(renderer, &w, &h);

		// Setup y bounds for whole screen
		int* y0 = malloc(sizeof(int) * w);
		int* y1 = malloc(sizeof(int) * w);
		for (int i = 0; i < w; i++) y0[i] = 0;
		for (int i = 0; i < w; i++) y1[i] = h;
		
		render_room(renderer, &camera, camera.room_idx, map, whole_screen(w), w, h, y0, y1);

		free(y0); free(y1);
	
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	free_map(map);
	return 0;
}
