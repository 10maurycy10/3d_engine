#include "map.h"
#include <assert.h>
#include <SDL2/SDL.h>

// How far from the center of the viewing plain (1 unit away from camera) should the screen be?
#define FOV .4

/////////////////////////
// Graphics Primitives //
/////////////////////////

void vline(SDL_Renderer* canvas, int x, int y0, int y1, int r, int g, int b) {
	// Using SDL's drawing functions here feals like cheating, but avoids having to manage my own pixel buffer.
	SDL_SetRenderDrawColor(canvas, r, g, b, 255);
	if (y1 > y0) SDL_RenderDrawLine(canvas, x, y0, x, y1);
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

// This computes how much of a wall is visable, storing the start and end in w1 and w0
// Returns false if the wall is fully outside, true otherwize
// The does not currently check against 
int clip_to_frustum(Point2* w0, Point2* w1, float fov) {
	float near_plane = 0.001;

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

	return 1;
}

//////////////
// Renderer //
////////////// 

// The main rendering function, renders a room (roomid) from the the point of view of the camera, to canvas.
// It will recurse to draw portals, so any connecting geometry visable trough the room is also drawn.
// All drawing is within the x bounds given by the x_min and x_max and the y bounds in x_min and y_max.
// h and w are the screen dimentions.
void render_room(SDL_Renderer* canvas, struct Camera* camera, int roomid, struct Map* map, int w, int h, int x_min, int x_max, int y_min[], int y_max[]) {
	// Recaculate the sin and cos of the camera angle
	camera->angle_cos = cos(camera->angle);
	camera->angle_sin = sin(camera->angle);
	
	struct Room* room = map->rooms[roomid];

	// Draw every wall in a room
	for (int wallid = 0; wallid < room->length; wallid++) {
		// Find the endpoints of the wall
		struct WallVertex* w0 = &room->walls[wallid];
		struct WallVertex* w1;
		if (wallid+1 < room->length) {
			w1 = &room->walls[wallid + 1];
		} else {
			w1 = &room->walls[0];
		}

		// Transform into camera relative cordinates
		Point2 p0 = world_to_camera_space(camera, w0->location);	
		Point2 p1 = world_to_camera_space(camera, w1->location);	
		
		// Don't render walls if behind the camera
		if (!clip_to_frustum(&p0, &p1, 0.4)) continue;
		
		// Project wall endpoints to screen space
		Point2 w0_upper = camera_to_pixel_space(p0, room->z1 - camera->z, h, w, FOV);
		Point2 w0_lower = camera_to_pixel_space(p0, room->z0 - camera->z, h, w, FOV);
		Point2 w1_upper = camera_to_pixel_space(p1, room->z1 - camera->z, h, w, FOV);
		Point2 w1_lower = camera_to_pixel_space(p1, room->z0 - camera->z, h, w, FOV);
		
		// In the case of portals, do some more projection to find where the top and bottom portions of the portal should be
		Point2 portal0_lower, portal0_upper, portal1_lower, portal1_upper;
		if (w0->portal_idx != -1) {
			int portal = w0->portal_idx;
			float bottom_height = MAX(0, map->rooms[portal]->z0 - room->z0);
			float top_height = MAX(0, room->z1 - map->rooms[portal]->z1);
			portal0_lower = camera_to_pixel_space(p0, room->z0 - camera->z + bottom_height, h, w, FOV);
			portal0_upper = camera_to_pixel_space(p0, room->z1 - camera->z - top_height, h, w, FOV);
			portal1_lower = camera_to_pixel_space(p1, room->z0 - camera->z + bottom_height, h, w, FOV);
			portal1_upper = camera_to_pixel_space(p1, room->z1 - camera->z - top_height, h, w, FOV);
		}

		// Limit the draw portion of the wall to the screen
		int x0 = MAX(x_min, w0_upper.x);
		int x1 = MIN(x_max, w1_upper.x);
		
		// Dont draw walls facing away from the player, or with zero size.
		if (x0 >= x1) continue;

		// For every pixel along the wall, draw the floor, ceiling, and the wal
		for (int x = x0; x < x1; x++) {
			int pixels_drawn = x - w0_upper.x;
			float part_drawn = (float)pixels_drawn / (w1_upper.x - w0_upper.x);

			// Interpolate the start and end y cordinates
			float y0 = lerp(w0_upper.y, w1_upper.y, part_drawn);
			float y1 = lerp(w0_lower.y, w1_lower.y, part_drawn);

			// Limit them to within the y bounds
			y0 = MAX(y_min[x], y0); y1 = MIN(y_max[x], y1);

			// Draw in the floor and ceiling, and in the case of a normal wall, draw it in.
			vline(canvas, x, y_min[x], y0, 0, 0, 64);
			vline(canvas, x, y1, y_max[x], 64, 64, 64);
			if (w0->portal_idx == -1) vline(canvas, x, y0, y1, w0->r, w0->g, w0->b);

			// In the case of a portal, draw the upper and lower segments
			if (w0->portal_idx != -1) {
				// Interpolate the y of the top and bottom of the portal
				float top_y = lerp(portal0_upper.y, portal1_upper.y, part_drawn);
				float bottom_y = lerp(portal0_lower.y, portal1_lower.y, part_drawn);
				
				// Limit to the bounds
				top_y = MAX(top_y, y_min[x]); bottom_y = MIN(bottom_y, y_max[x]);
				
				// Draw the top and bottom
				vline(canvas, x, y0, top_y, w0->r, w0->g, w0->b);
				vline(canvas, x, bottom_y, y1, w0->r, w0->g, w0->b);
			
				// Update the bounds
				y_min[x] = top_y;
				y_max[x] = bottom_y;
			}
		}
		
		if (w0->portal_idx != -1) {
			// Recurse to draw objects beond a portal
			// The x bounds are simply the space that the portal would have been drawn in if it was a wall
			// The y bounds are set while drawing the floor, ceiling and top and bottom sections.
			render_room(canvas, camera, w0->portal_idx, map, w, h, x0, x1, y_min, y_max);
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
	// Check if the user wants to close the window
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				exit(0);
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
	// Simple function to check collisions between old_location and camera->location,
	// updateing camera->location and camera->room_idx as needed
	void check_collide() {
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
	}

	// Movement is split between the x and y directions so that the player doesnt get snagged on walls	
	float angle = -camera->angle;
	camera->location.x += translation.x * cos(angle) - translation.y * sin(angle);
	
	check_collide();
	old_location = camera->location;

	camera->location.y += translation.x * sin(angle) + translation.y * cos(angle);
	
	check_collide();

	// Update camera z to be 1 unit above the floor.
	camera->z = map->rooms[camera->room_idx]->z0 + 1;
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
		
		render_room(renderer, &camera, camera.room_idx, map, w, h, 0, w, y0, y1);

		free(y0); free(y1);
	
		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	free_map(map);
	return 0;
}
