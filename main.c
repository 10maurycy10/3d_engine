#include "map.h"
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define float double

//SDL_Surface* textures[2];

// How far from the center of the viewing plain (1 unit away from camera) should the screen be?
#define FOV .4

// Flag for slow rendering
int slow_render = 0;


/////////////////////////
// Graphics Primitives //
/////////////////////////

// A wrapper for a window, and a pixel buffer for rendering
// Drawing should be done to the canvas surface.
typedef struct Window {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Surface* canvas;
	SDL_Texture* canvas_texture;
	int w, h;
} Window;

// Open a window an create a Window stuct
Window window_open() {
//	int windowFlags = SDL_WINDOW_RESIZABLE;
	int windowFlags = 0;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_Window* window = SDL_CreateWindow("3d", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640*2, 480*2, windowFlags);

	if (!window) {
		printf("Failed to open window: %s\n", SDL_GetError());
		exit(1);
	}
	
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC); 
	
	if (!renderer) {
		printf("Failed to open renderer: %s\n", SDL_GetError());
		exit(1);
	};

	return (Window) {
		.window = window,
		.w = 640,
		.h = 480,
		.renderer = renderer,
		.canvas = NULL,
		.canvas_texture = NULL,
	};
}

// (re)prepare a buffer for rendering, does nothing if it is already initalized at the same resolution
void renderer_setup(Window* window, int w, int h) {
	// Do nothing if the current resolution is the same, and the struct is initalized.
	if (w == window->w && h == window->h && window->canvas && window->canvas_texture) return;

	window->w = w;
	window->h = h;

	if (window->canvas) SDL_FreeSurface(window->canvas);
	if (window->canvas_texture) SDL_DestroyTexture(window->canvas_texture);
	
	window->canvas = SDL_CreateRGBSurface(0, h, w, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
	assert(window->canvas);
	window->canvas_texture = SDL_CreateTexture(window->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, h, w);
	assert(window->canvas_texture);
}

// Show the graphics draw in the pixel buffer to the screen
void window_present(Window window) {
	// Copy rendered graphics to the to the gpu
	void* texture_pixels;
	int texture_pitch;
	SDL_LockTexture(window.canvas_texture, NULL, &texture_pixels, &texture_pitch);
	memcpy(texture_pixels, window.canvas->pixels, window.canvas->pitch * window.canvas->h);
	SDL_UnlockTexture(window.canvas_texture);

	// Draw the texture onto the renderer 
	SDL_RenderCopy(window.renderer, window.canvas_texture, NULL, NULL);
	
	// Present the renderer
	SDL_RenderPresent(window.renderer);
}

void vline(SDL_Surface* canvas, int x, int y0, int y1, int r, int g, int b) {
	for (int y = y0; y < y1; y++)
		((uint32_t*)canvas->pixels)[x + y * canvas->w] = r << 24 | g << 16 | b << 8 | 0xff;
}

// Draw a textured line
// x, y0, and y1 are the phisical area of the line
// texture_x, texture_y0, texture_y1 are the texture cordinates for the start and end
// y0_orig and y1_orig are the on screen y locations of texture_y0 and texture_y1, this makes applying bounds easyer,
// Just clip y0 and y1 but not y0_orig or y1_orig to clip the line while preserving texture layout
void textured_vline(
	SDL_Surface* canvas, 
	int x, int y0, int y1,
	int y0_orig, int y1_orig, float texture_x, float texture_y0, float texture_y1, 
	SDL_Surface* texture
) {
	int texture_pixel_x = abs(texture_x * texture -> w)%texture->w;
	int texture_pixel_y0 = texture_y0 * texture -> h;
	int texture_pixel_y1 = texture_y1 * texture -> h;
	for (int y = y0; y < y1; y++) {
		int i = y0_orig - y;
		float distance = (float)i / (float)(y1_orig - y0_orig);
		int texture_pixel_y = lerp(texture_pixel_y0, texture_pixel_y1, distance);
		texture_pixel_y %= texture->h;
		texture_pixel_y = abs(texture_pixel_y);
		uint32_t* canvas_pixel = &((uint32_t*)canvas->pixels)[x + y * canvas->w];
		uint32_t* texture_pixel = &((uint32_t*)texture->pixels)[texture_pixel_x + texture_pixel_y * texture->w];
		*canvas_pixel = *texture_pixel;
	}
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

// Convert a camera space point to a world space poimt
Point2 camera_to_world_space(struct Camera* camera, Point2 c) {
	Point2 rotated =  {
		.x = c.x * cos(-camera->angle) - c.y * sin(-camera->angle),
		.y = c.x * sin(-camera->angle) + c.y * cos(-camera->angle)
	};
	
	return (Point2) {.x = rotated.x + camera->location.x, .y = rotated.y + camera->location.y };
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

Point2 get_texture_cordinates(Point2 point, float z) {
	return (Point2) {
		.x = point.x + point.y,
		.y = z + 0.25
	};
} 

/////////////
// Cliping //
/////////////

// This computes how much of a wall is visable, storing the start and end in w1 and w0
// Returns false if the wall is fully outside, true otherwize
// The does not currently check against 
int clip_to_frustum(Point2* w0, Point2* w1, float fov) {
	float near_plane = 0.00001;

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

//////////////
// Renderer //
////////////// 

// The main rendering function, renders a room (roomid) from the the point of view of the camera, to canvas.
// It will recurse to draw portals, so any connecting geometry visable trough the room is also drawn.
// All drawing is within the x bounds given by the x_min and x_max and the y bounds in x_min and y_max.
// h and w are the screen dimentions.
void render_room(Window* window, struct Camera* camera, int roomid, struct Map* map, int x_min, int x_max, int y_min[], int y_max[]) {
	int h = window->h;
	int w = window->w;
	SDL_Surface* canvas = window->canvas;
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

		Point2 uv0_upper = get_texture_cordinates(camera_to_world_space(camera, p0), room->z1);
		Point2 uv0_lower = get_texture_cordinates(camera_to_world_space(camera, p0), room->z0);
		Point2 uv1_upper = get_texture_cordinates(camera_to_world_space(camera, p1), room->z1);
		Point2 uv1_lower = get_texture_cordinates(camera_to_world_space(camera, p1), room->z0);
		
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
		float x0 = MAX((float)x_min, w0_upper.x);
		float x1 = MIN((float)x_max, w1_upper.x);
		
		// Dont draw walls facing away from the player, or with zero size.
		if (x0 >= x1) continue;

		// For every pixel along the wall, draw the floor, ceiling, and the wal
		for (int x = x0; x < x1; x++) {
			int pixels_drawn = x - (int)w0_upper.x;
			float part_drawn = (float)pixels_drawn / (w1_upper.x - w0_upper.x);

			// Interpolate the start and end y cordinates
			float y0_unclamped = lerp(w0_upper.y, w1_upper.y, part_drawn);
			float y1_unclamped = lerp(w0_lower.y, w1_lower.y, part_drawn);

			Point2 uv_upper = {
				.x = lerp(uv0_upper.x, uv1_upper.x, part_drawn),
				.y = lerp(uv0_upper.y, uv1_upper.y, part_drawn),
			};
			Point2 uv_lower = {
				.x = lerp(uv0_lower.x, uv1_lower.x, part_drawn),
				.y = lerp(uv0_lower.y, uv1_lower.y, part_drawn),
			};

			// Limit them to within the y bounds
			float y0 = MAX(y_min[x], y0_unclamped);
			float y1 = MIN(y_max[x], y1_unclamped);

			// Draw in the floor and ceiling, and in the case of a normal wall, draw it in.
			vline(canvas, x, y_min[x], y0, 0, 0, 64);
			vline(canvas, x, y1, y_max[x], 64, 64, 64);
			if (w0->portal_idx == -1) {
//				if (w0->texture != -1) {
//					textured_vline(canvas, x, y0, y1, y0_unclamped, y1_unclamped, uv_upper.x, uv_upper.y, uv_lower.y, textures[w0->texture]);
//					textured_vline(canvas, x, y0, y1, y0_unclamped, y1_unclamped, part_drawn, 1, 0, textures[w0->texture]);
//				} else {
					vline(canvas, x, y0, y1, w0->r, w0->g, w0->b);
//				}
			}

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

			if (slow_render) {
				window_present(*window);
				SDL_Delay(10);
			}
		}
		
		if (w0->portal_idx != -1) {
			// Recurse to draw objects beond a portal
			// The x bounds are simply the space that the portal would have been drawn in if it was a wall
			// The y bounds are set while drawing the floor, ceiling and top and bottom sections.
			render_room(window, camera, w0->portal_idx, map, x0, x1, y_min, y_max);
		}
	}
}

/////////////
// UI code //
/////////////

void do_input(struct Map* map, struct Camera* camera) {
	// Check if the user wants to close the window
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				exit(0);
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.scancode == SDL_SCANCODE_TAB) {
					slow_render ^= 1;
				}
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

	//textures[1] = IMG_Load("textures/stone.png");
	//textures[0] = IMG_Load("textures/wood.png");

	//assert(textures[0]);
	//assert(textures[1]);

	//textures[0] = SDL_ConvertSurfaceFormat(textures[0], SDL_PIXELFORMAT_RGBA8888, 0);
	//textures[1] = SDL_ConvertSurfaceFormat(textures[1], SDL_PIXELFORMAT_RGBA8888, 0);

	char* mapfile = argv[1];

	// Open a window,
	Window window = window_open();
	
	struct Map* map = load_map_from_file(fopen(mapfile, "r"));
	struct Camera camera = {.location = map->starting_location, .room_idx=map->starting_room, .z=0};

	while (1) {
		// Handle inputs
		do_input(map, &camera);
			
		// Sanity check, make sure the player has a valid room
		assert(map->length > camera.room_idx);
	
		// Get size of the window, and ensure that the rendering buffer is the same size.
		// int h = 720, w = 1020;
		// SDL_GetRendererOutputSize(window.renderer, &w, &h);

		// Doom resolution :)
		int h = 640/2, w = 480/2;
		renderer_setup(&window, h, w);
	
		// Fill viewport with hot pink to make unrendered areas easly visiable
		SDL_FillRect(window.canvas, NULL, 0xff00ffff);

		// Render to the canvas surface
		SDL_LockSurface(window.canvas);
		// Initalize bounds for rendering
		int* y0 = malloc(sizeof(int) * w);
		int* y1 = malloc(sizeof(int) * w);
		for (int i = 0; i < w; i++) y0[i] = 0;
		for (int i = 0; i < w; i++) y1[i] = h;
		// Render!
		render_room(&window, &camera, camera.room_idx, map, 0, w, y0, y1);
		// Clean up
		free(y0); free(y1);
		SDL_UnlockSurface(window.canvas);
		
		window_present(window);
	}

	free_map(map);
	return 0;
}
